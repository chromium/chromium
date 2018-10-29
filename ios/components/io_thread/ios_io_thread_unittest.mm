// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/io_thread/ios_io_thread.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/url_request_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  IOSIOThreadTest() : thread_bundle_(web::TestWebThreadBundle::IO_MAINLOOP) {
    net::URLRequestFailedJob::AddUrlHandler();
  }

  ~IOSIOThreadTest() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

 private:
  web::TestWebThreadBundle thread_bundle_;
};

// Tests the creation of an IOSIOThread and verifies that it returns a system
// url request context.
TEST_F(IOSIOThreadTest, AssertSystemUrlRequestContext) {
  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(base::MakeRefCounted<TestingPrefStore>());

  scoped_refptr<PrefRegistrySimple> pref_registry = new PrefRegistrySimple;
  PrefProxyConfigTrackerImpl::RegisterPrefs(pref_registry.get());

  std::unique_ptr<PrefService> pref_service(
      pref_service_factory.Create(pref_registry.get()));

  std::unique_ptr<TestIOThread> test_io_thread(
      new TestIOThread(pref_service.get(), nullptr));

  ASSERT_TRUE(test_io_thread->system_url_request_context_getter());
}
