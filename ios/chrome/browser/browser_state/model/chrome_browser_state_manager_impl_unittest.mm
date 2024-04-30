// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_state/model/chrome_browser_state_manager_impl.h"

#import "ios/chrome/browser/browser_state/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class ChromeBrowserStateManagerImplTest : public PlatformTest {
 public:
  ChromeBrowserStateManagerImplTest()
      : web_task_environment_(
            web::WebTaskEnvironment::IOThreadType::REAL_THREAD_DELAYED) {
    TestingApplicationContext* application_context =
        TestingApplicationContext::GetGlobal();

    // IOSChromeIOThread needs to be created before the IO thread is started.
    // Thus DELAY_IO_THREAD_START is set in WebTaskEnvironment's options. The
    // thread is then started after the creation of IOSChromeIOThread.
    chrome_io_ = std::make_unique<IOSChromeIOThread>(
        application_context->GetLocalState(), application_context->GetNetLog());

    // Register the objects with the TestingApplicationContext.
    application_context->SetIOSChromeIOThread(chrome_io_.get());
    application_context->SetChromeBrowserStateManager(&browser_state_manager_);

    // Start the IO thread.
    web_task_environment_.StartIOThread();

    // Post a task to initialize the IOSChromeIOThread object on the IO thread.
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&IOSChromeIOThread::InitOnIO,
                                  base::Unretained(chrome_io_.get())));

    // Init the BrowserPolicyConnect as this is required to create
    // ChromeBrowserStateImpl.
    application_context->GetBrowserPolicyConnector()->Init(
        application_context->GetLocalState(),
        application_context->GetSharedURLLoaderFactory());

    // IOSChromeIOThread requires the SystemURLRequestContextGetter() to be
    // created before the object is shutdown, so force its creation here.
    std::ignore = chrome_io_->system_url_request_context_getter();
  }

  ~ChromeBrowserStateManagerImplTest() override {
    TestingApplicationContext* application_context =
        TestingApplicationContext::GetGlobal();

    application_context->GetBrowserPolicyConnector()->Shutdown();
    application_context->GetIOSChromeIOThread()->NetworkTearDown();
    application_context->SetChromeBrowserStateManager(nullptr);
    application_context->SetIOSChromeIOThread(nullptr);
  }

  ChromeBrowserStateManagerImpl& browser_state_manager() {
    return browser_state_manager_;
  }

 private:
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<IOSChromeIOThread> chrome_io_;
  web::WebTaskEnvironment web_task_environment_;
  ChromeBrowserStateManagerImpl browser_state_manager_;
};

// Tests that GetLoadedBrowserStates() returns an empty list before the
// BrowserStates are loaded, and then a list containing at least one
// BrowserState
TEST_F(ChromeBrowserStateManagerImplTest, GetLoadedBrowserStates) {
  // There should be non-BrowserState loaded yet.
  EXPECT_EQ(browser_state_manager().GetLoadedBrowserStates().size(), 0u);

  // Load the BrowserStates, this will implicitly add "Default" as a
  // BrowserState if there is no saved BrowserStates. Thus it should
  // load exactly one BrowserState.
  browser_state_manager().LoadBrowserStates();
  EXPECT_EQ(browser_state_manager().GetLoadedBrowserStates().size(), 1u);
}
