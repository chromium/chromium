// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_client.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "jingle/glue/network_service_config_test_util.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

namespace {

class PushClientTest : public testing::Test {
 protected:
  PushClientTest()
      : net_config_helper_(
            base::MakeRefCounted<net::TestURLRequestContextGetter>(
                task_environment_.GetMainThreadTaskRunner())) {
    net_config_helper_.FillInNetworkConfig(&notifier_options_.network_config);
  }

  ~PushClientTest() override {}

  // The sockets created by the XMPP code expect an IO loop.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  jingle_glue::NetworkServiceConfigTestUtil net_config_helper_;
  NotifierOptions notifier_options_;
};

// Make sure calling CreateDefault on the IO thread doesn't blow up.
TEST_F(PushClientTest, CreateDefaultOnIOThread) {
  const std::unique_ptr<PushClient> push_client(
      PushClient::CreateDefault(notifier_options_));
}

// Make sure calling CreateDefault on a non-IO thread doesn't blow up.
TEST_F(PushClientTest, CreateDefaultOffIOThread) {
  base::Thread thread("Non-IO thread");
  EXPECT_TRUE(thread.Start());
  thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&PushClient::CreateDefault),
                                notifier_options_));
  thread.Stop();
}

// Make sure calling CreateDefaultOnIOThread on the IO thread doesn't
// blow up.
TEST_F(PushClientTest, CreateDefaultOnIOThreadOnIOThread) {
  const std::unique_ptr<PushClient> push_client(
      PushClient::CreateDefaultOnIOThread(notifier_options_));
}

}  // namespace

}  // namespace notifier
