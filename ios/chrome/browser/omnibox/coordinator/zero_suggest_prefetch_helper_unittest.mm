// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/coordinator/zero_suggest_prefetch_helper.h"

#import "base/test/task_environment.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/fake_autocomplete_provider_client.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_client.h"
#import "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/OCMock/OCMockObject.h"
#import "third_party/ocmock/gtest_support.h"

using web::FakeWebState;

namespace {

const char kTestURL[] = "http://chromium.org";
const char kTestSRPURL[] = "https://www.google.com/search?q=omnibox";

}  // namespace

namespace {

class ZeroSuggestPrefetchHelperTest : public PlatformTest {
 public:
  ~ZeroSuggestPrefetchHelperTest() override { [helper_ disconnect]; }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);

    mock_controller_ =
        [OCMockObject mockForClass:OmniboxAutocompleteController.class];
  }

  void CreateHelper() {
    helper_ = [[ZeroSuggestPrefetchHelper alloc]
        initWithWebStateList:web_state_list_.get()];
    helper_.omniboxAutocompleteController = mock_controller_;
  }
  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;

  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;

  // Mock OmniboxAutocompleteController.
  id mock_controller_;

  ZeroSuggestPrefetchHelper* helper_;
};

// Test that upon navigation, prefetch is called.
TEST_F(ZeroSuggestPrefetchHelperTest, TestReactToNavigation) {
  CreateHelper();
  web::FakeNavigationContext context;

  GURL not_ntp_url(kTestURL);
  auto web_state = std::make_unique<web::FakeWebState>();
  [[mock_controller_ expect] startZeroSuggestPrefetch];
  FakeWebState* web_state_ptr = web_state.get();
  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(mock_controller_);

  [[mock_controller_ expect] startZeroSuggestPrefetch];
  web_state_ptr->OnNavigationFinished(&context);
  EXPECT_OCMOCK_VERIFY(mock_controller_);

  // Now navigate to NTP.
  [[mock_controller_ expect] startZeroSuggestPrefetch];
  GURL url(kChromeUINewTabURL);
  web_state_ptr->SetCurrentURL(url);
  web_state_ptr->OnNavigationFinished(&context);
  EXPECT_OCMOCK_VERIFY(mock_controller_);

  [[mock_controller_ expect] startZeroSuggestPrefetch];
  web_state_ptr->SetCurrentURL(GURL(kTestSRPURL));
  web_state_ptr->OnNavigationFinished(&context);
  EXPECT_OCMOCK_VERIFY(mock_controller_);
}

// Test that switching between tabs starts prefetch.
TEST_F(ZeroSuggestPrefetchHelperTest, TestPrefetchOnTabSwitch) {
  CreateHelper();
  web::FakeNavigationContext context;

  GURL not_ntp_url(kTestURL);
  auto web_state = std::make_unique<web::FakeWebState>();
  FakeWebState* web_state_ptr = web_state.get();

  [[mock_controller_ expect] startZeroSuggestPrefetch];
  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(mock_controller_);

  [[mock_controller_ expect] startZeroSuggestPrefetch];
  web_state_ptr->OnNavigationFinished(&context);
  EXPECT_OCMOCK_VERIFY(mock_controller_);

  // Second tab
  web_state = std::make_unique<web::FakeWebState>();
  web_state_ptr = web_state.get();

  [[mock_controller_ expect] startZeroSuggestPrefetch];
  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(1);
  EXPECT_OCMOCK_VERIFY(mock_controller_);

  // Just switch
  [[mock_controller_ expect] startZeroSuggestPrefetch];
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(mock_controller_);
}

// Test that the appropriate behavior (set `is_background_state` variable, start
// prefetch, etc.) is triggered when the app is foregrounded/backgrounded.
TEST_F(ZeroSuggestPrefetchHelperTest,
       TestReactToForegroundingAndBackgrounding) {
  CreateHelper();
  web::FakeNavigationContext context;

  // Initialize the WebState machinery for proper verification of ZPS prefetch
  // request counts.
  auto web_state = std::make_unique<web::FakeWebState>();
  FakeWebState* web_state_ptr = web_state.get();
  [[mock_controller_ expect] startZeroSuggestPrefetch];
  web_state_ptr->SetCurrentURL(GURL(kTestURL));
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(mock_controller_);

  // Initially the app starts off in the foreground state.
  [[mock_controller_ expect] setBackgroundStateForProviders:YES];

  // Receiving a "backgrounded" notification will cause the app to move to the
  // background state.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];
  EXPECT_OCMOCK_VERIFY(mock_controller_);

  [[mock_controller_ expect] startZeroSuggestPrefetch];
  [[mock_controller_ expect] setBackgroundStateForProviders:NO];

  // Receiving a "foregrounded" notification will cause the app to move to the
  // foreground state (triggering a ZPS prefetch request as a side-effect).
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillEnterForegroundNotification
                    object:nil];
  EXPECT_OCMOCK_VERIFY(mock_controller_);
}

}  // namespace
