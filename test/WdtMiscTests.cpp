/**
 * Copyright (c) 2014-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#include "TestCommon.h"

#include "common/files/FileUtil.h"

#include <wdt/Wdt.h>
#include <thread>

using namespace std;

namespace facebook {
namespace wdt {

TEST(BasicTest, ReceiverAcceptTimeout) {
  Wdt &wdt = Wdt::initializeWdt("unit test app");
  WdtOptions &opts = wdt.getWdtOptions();
  opts.accept_timeout_millis = 1;
  opts.max_accept_retries = 1;
  opts.max_retries = 1;
  WdtTransferRequest req;
  //{
  Receiver r(0, 2, "/tmp/wdtTest");
  req = r.init();
  EXPECT_EQ(OK, r.transferAsync());
  auto report = r.finish();
  EXPECT_EQ(CONN_ERROR, report->getSummary().getErrorCode());
  //}
  // Receiver object is still alive but has given up - we should not be able
  // to connect:
  req.directory = "/bin";
  EXPECT_EQ(CONN_ERROR, wdt.wdtSend("foo", req));
}

TEST(BasicTest, MultiWdtSender) {
  string srcDir = "/tmp/wdtSrc";
  string srcFile = "/tmp/wdtSrc/srcFile";
  string targetDir = "/tmp/wdtTest";
  files::FileUtil::recreateDir(srcDir);
  files::FileUtil::recreateDir(targetDir);

  {
    // Create 400mb srcFile
    const uint size = 1024 * 1024;
    uint a[size];
    FILE *pFile;
    pFile = fopen(srcFile.c_str(), "wb");
    for (int i = 0; i < 100; ++i) {
      fwrite(a, 1, size * sizeof(uint), pFile);
    }
    fclose(pFile);
  }

  Wdt &wdt = Wdt::initializeWdt("unit test app");
  WdtOptions &options = wdt.getWdtOptions();
  options.avg_mbytes_per_sec = 100;
  WdtTransferRequest req(/* start port */ 0, /* num ports */ 1, targetDir);
  Receiver r(req);
  req = r.init();
  EXPECT_EQ(OK, r.transferAsync());
  req.directory = string(srcDir);
  auto sender1Thread = thread([&wdt, &req]() {
    EXPECT_EQ(OK, wdt.wdtSend("foo", req, nullptr, true));
  });
  auto sender2Thread = thread([&wdt, &req]() {
    /* sleep override */
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(ALREADY_EXISTS, wdt.wdtSend("foo", req, nullptr, true));
  });
  sender1Thread.join();
  sender2Thread.join();

  EXPECT_EQ(OK, r.finish()->getSummary().getErrorCode());
  files::FileUtil::removeAll(srcDir);
  files::FileUtil::removeAll(targetDir);
}
}
}  // namespace end

int main(int argc, char *argv[]) {
  FLAGS_logtostderr = true;
  testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  int ret = RUN_ALL_TESTS();
  return ret;
}
