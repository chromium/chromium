// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/io_thread/ios_io_thread.h"

#import <memory>

#import "base/test/task_environment.h"
#import "components/prefs/testing_pref_service.h"
#import "components/proxy_config/pref_proxy_config_tracker_impl.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/test_web_thread.h"
#import "ios/web/web_thread_impl.h"
#import "net/base/network_delegate.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// A concrete implementation of IOThread for testing.
class TestIOThread : public io_thread::IOSIOThread {
 public:
  TestIOThread(PrefService* local_state, net::NetLog* net_log)
      : IOSIOThread(local_state, net_log) {}
  ~TestIOThread() override {}

  // Dummy implementations of virtual methods.
  std::unique_ptr<net::NetworkDelegate> CreateSystemNetworkDelegate() override {
    return nullptr;
  }
  std::string GetChannelString() const override { return std::string(); }
};

}  // namespace

class IOSIOThreadTest : public PlatformTest {
 public:
  IOSIOThreadTest() : web_client_(std::make_unique<web::FakeWebClient>()) {
    web::WebThreadImpl::CreateTaskExecutor();

    ui_thread_ = std::make_unique<web::TestWebThread>(
        web::WebThread::UI, task_environment_.GetMainThreadTaskRunner());
  }

  ~IOSIOThreadTest() override {
    web::WebThreadImpl::ResetTaskExecutorForTesting();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<web::TestWebThread> ui_thread_;
};

// Tests the creation of an IOSIOThread and verifies that it returns a system
// url request context.
TEST_F(IOSIOThreadTest, AssertSystemUrlRequestContext) {
  std::unique_ptr<TestingPrefServiceSimple> pref_service(
      std::make_unique<TestingPrefServiceSimple>());
  PrefProxyConfigTrackerImpl::RegisterPrefs(pref_service->registry());

  // Create the IO thread but do not register it yet.
  std::unique_ptr<web::TestWebThread> io_thread(
      std::make_unique<web::TestWebThread>(web::WebThread::IO));
  io_thread->StartIOThreadUnregistered();

  // Create the TestIOThread before the IO thread is registered.
  std::unique_ptr<TestIOThread> ios_io_thread(
      new TestIOThread(pref_service.get(), nullptr));
  io_thread->RegisterAsWebThread();

  ASSERT_TRUE(ios_io_thread->system_url_request_context_getter());

  // Explicitly destroy the IO thread so that it is unregistered before the
  // TestIOThread is destroyed.
  io_thread.reset();
}
