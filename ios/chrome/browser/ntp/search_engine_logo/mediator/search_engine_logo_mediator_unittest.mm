// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

class SearchEngineLogoMediatorTest : public PlatformTest {
 protected:
  SearchEngineLogoMediatorTest()
      : web_state_(std::make_unique<web::FakeWebState>()) {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(test_profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());

    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    controller_ =
        [[SearchEngineLogoMediator alloc] initWithBrowser:browser_.get()
                                                 webState:web_state_.get()];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<web::WebState> web_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  SearchEngineLogoMediator* controller_;
};

// Sanity check.
TEST_F(SearchEngineLogoMediatorTest, TestConstructorDestructor) {
  EXPECT_TRUE(controller_);
}

// Verifies that tapping the doodle navigates to the stored URL.
TEST_F(SearchEngineLogoMediatorTest, TestTapDoodleWithValidClickURL) {
  const std::string kURL = "http://foo/";
  [controller_ setClickURLText:GURL(kURL)];

  // Tap the doodle and verify the expected url was loaded.
  [controller_ simulateDoodleTapped];
  EXPECT_EQ(kURL, url_loader_->last_params.web_params.url);
  EXPECT_EQ(1, url_loader_->load_current_tab_call_count);
}

// Verifies the case where the URL value is invalid (which should result in not
// attempting to load any URL when the doodle is tapped).
TEST_F(SearchEngineLogoMediatorTest, TestTapDoodle_InvalidSearchQuery) {
  [controller_ setClickURLText:GURL("foo")];

  // Tap the doodle and verify nothing was loaded.
  [controller_ simulateDoodleTapped];

  EXPECT_EQ(GURL(), url_loader_->last_params.web_params.url);
  EXPECT_EQ(0, url_loader_->load_current_tab_call_count);
}

}  // anonymous namespace
