// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/zero_suggest_prefetch_helper.h"

#import "base/test/task_environment.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/fake_autocomplete_provider_client.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_client.h"
#import "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"
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

using testing::Return;
using web::FakeWebState;

namespace {

const char kTestURL[] = "http://chromium.org";
const char kTestSRPURL[] = "https://www.google.com/search?q=omnibox";

// A mock class for the AutocompleteController.
class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController()
      : AutocompleteController(
            std::make_unique<FakeAutocompleteProviderClient>(),
            0) {}
  MockAutocompleteController(const MockAutocompleteController&) = delete;
  MockAutocompleteController& operator=(const MockAutocompleteController&) =
      delete;
  ~MockAutocompleteController() override = default;
};

class TestOmniboxController : public OmniboxController {
 public:
  TestOmniboxController(OmniboxView* view,
                        std::unique_ptr<OmniboxClient> client)
      : OmniboxController(view, std::move(client)) {}

  ~TestOmniboxController() override = default;
  TestOmniboxController(const TestOmniboxController&) = delete;
  TestOmniboxController& operator=(const TestOmniboxController&) = delete;

  // OmniboxController:
  void StartZeroSuggestPrefetch() override { start_prefetch_call_count_++; }

  int start_prefetch_call_count_ = 0;
};

}  // namespace

namespace {

class ZeroSuggestPrefetchHelperTest : public PlatformTest {
 public:
  ~ZeroSuggestPrefetchHelperTest() override { [helper_ disconnect]; }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);

    auto omnibox_client = std::make_unique<TestOmniboxClient>();

    controller_ = std::make_unique<TestOmniboxController>(
        /*view=*/nullptr, std::move(omnibox_client));
    controller_->SetAutocompleteControllerForTesting(
        std::make_unique<MockAutocompleteController>());
  }

  void CreateHelper() {
    helper_ = [[ZeroSuggestPrefetchHelper alloc]
        initWithWebStateList:web_state_list_.get()
                  controller:controller_.get()];
  }
  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;

  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;

  std::unique_ptr<TestOmniboxController> controller_;

  ZeroSuggestPrefetchHelper* helper_;
};

// Test that upon navigation, prefetch is called.
TEST_F(ZeroSuggestPrefetchHelperTest, TestReactToNavigation) {
  CreateHelper();
  web::FakeNavigationContext context;

  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 0);
  GURL not_ntp_url(kTestURL);
  auto web_state = std::make_unique<web::FakeWebState>();
  FakeWebState* web_state_ptr = web_state.get();
  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 1);
  web_state_ptr->OnNavigationFinished(&context);
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 2);
  controller_.get()->start_prefetch_call_count_ = 0;

  // Now navigate to NTP.
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 0);

  GURL url(kChromeUINewTabURL);
  web_state_ptr->SetCurrentURL(url);
  web_state_ptr->OnNavigationFinished(&context);

  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 1);
  controller_.get()->start_prefetch_call_count_ = 0;

  // Now navigate to SRP.
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 0);

  web_state_ptr->SetCurrentURL(GURL(kTestSRPURL));
  web_state_ptr->OnNavigationFinished(&context);

  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 1);
  controller_.get()->start_prefetch_call_count_ = 0;
}

// Test that switching between tabs starts prefetch.
TEST_F(ZeroSuggestPrefetchHelperTest, TestPrefetchOnTabSwitch) {
  CreateHelper();
  web::FakeNavigationContext context;

  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 0);
  GURL not_ntp_url(kTestURL);
  auto web_state = std::make_unique<web::FakeWebState>();
  FakeWebState* web_state_ptr = web_state.get();
  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 1);
  web_state_ptr->OnNavigationFinished(&context);
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 2);
  controller_.get()->start_prefetch_call_count_ = 0;

  // Second tab
  web_state = std::make_unique<web::FakeWebState>();
  web_state_ptr = web_state.get();
  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(1);
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 1);
  controller_.get()->start_prefetch_call_count_ = 0;

  // Just switch
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 1);
  controller_.get()->start_prefetch_call_count_ = 0;
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
  web_state_ptr->SetCurrentURL(GURL(kTestURL));
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(0);

  controller_.get()->start_prefetch_call_count_ = 0;

  // Initially the app starts off in the foreground state.
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 0);
  EXPECT_FALSE(controller_->autocomplete_controller()
                   ->autocomplete_provider_client()
                   ->in_background_state());

  // Receiving a "backgrounded" notification will cause the app to move to the
  // background state.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 0);
  EXPECT_TRUE(controller_.get()
                  ->autocomplete_controller()
                  ->autocomplete_provider_client()
                  ->in_background_state());

  // Receiving a "foregrounded" notification will cause the app to move to the
  // foreground state (triggering a ZPS prefetch request as a side-effect).
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillEnterForegroundNotification
                    object:nil];
  EXPECT_EQ(controller_.get()->start_prefetch_call_count_, 1);
  EXPECT_FALSE(controller_.get()
                   ->autocomplete_controller()
                   ->autocomplete_provider_client()
                   ->in_background_state());
}

}  // namespace
