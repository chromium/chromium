// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/zero_suggest_prefetch_helper.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/mock_autocomplete_provider_client.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_client.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_legacy.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::FakeWebState;

namespace {

const char kTestURL[] = "http://chromium.org";

class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController(
      std::unique_ptr<AutocompleteProviderClient> provider_client,
      int provider_types)
      : AutocompleteController(std::move(provider_client), provider_types) {}
  ~MockAutocompleteController() override = default;
  MockAutocompleteController(const MockAutocompleteController&) = delete;
  MockAutocompleteController& operator=(const MockAutocompleteController&) =
      delete;

  // AutocompleteController:
  MOCK_METHOD1(Start, void(const AutocompleteInput&));
  MOCK_METHOD1(StartPrefetch, void(const AutocompleteInput&));
};

}  // namespace

namespace {

class ZeroSuggestPrefetchHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    feature_list_.InitWithFeatures({omnibox::kZeroSuggestPrefetching}, {});

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);

    auto template_url_service = std::make_unique<TemplateURLService>(
        /*prefs=*/nullptr, std::make_unique<SearchTermsData>(),
        /*web_data_service=*/nullptr,
        std::unique_ptr<TemplateURLServiceClient>(), base::RepeatingClosure());
    auto client_ = std::make_unique<MockAutocompleteProviderClient>();
    client_->set_template_url_service(std::move(template_url_service));
    controller_ =
        std::make_unique<testing::NiceMock<MockAutocompleteController>>(
            std::move(client_), 0);
  }

  void CreateHelper() {
    helper_ = [[ZeroSuggestPrefetchHelper alloc]
          initWithWebStateList:web_state_list_.get()
        autocompleteController:controller_.get()];
  }

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;

  std::unique_ptr<testing::NiceMock<MockAutocompleteController>> controller_;

  ZeroSuggestPrefetchHelper* helper_;

  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;

  base::test::ScopedFeatureList feature_list_;
};

// Test that upon navigation, the prefetch happens when the new URL of the
// current tab is a NTP URL.
TEST_F(ZeroSuggestPrefetchHelperTest, TestReactToNavigation) {
  CreateHelper();

  EXPECT_CALL(*controller_, StartPrefetch).Times(0);
  EXPECT_CALL(*controller_, Start).Times(0);

  GURL not_ntp_url(kTestURL);
  auto web_state = std::make_unique<web::FakeWebState>();
  FakeWebState* web_state_ptr = web_state.get();
  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_list_->InsertWebState(
      0, std::move(web_state), WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  web_state_list_->ActivateWebStateAt(0);
  testing::Mock::VerifyAndClearExpectations(controller_.get());

  // Now navigate to NTP.
  EXPECT_CALL(*controller_, StartPrefetch).Times(1);
  EXPECT_CALL(*controller_, Start).Times(0);

  GURL url(kChromeUINewTabURL);
  web_state_ptr->SetCurrentURL(url);
  web::FakeNavigationContext context;
  web_state_ptr->OnNavigationFinished(&context);

  testing::Mock::VerifyAndClearExpectations(controller_.get());

  // Now navigate to non-NTP.
  EXPECT_CALL(*controller_, StartPrefetch).Times(0);
  EXPECT_CALL(*controller_, Start).Times(0);

  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_ptr->OnNavigationFinished(&context);

  testing::Mock::VerifyAndClearExpectations(controller_.get());
}

// Test switching between a NTP and a non-NTP tabs starts prefetch, but only
// when switching to NTP.
TEST_F(ZeroSuggestPrefetchHelperTest, TestSwitchNTPNonNTP) {
  CreateHelper();

  EXPECT_CALL(*controller_, StartPrefetch).Times(0);
  EXPECT_CALL(*controller_, Start).Times(0);

  auto web_state = std::make_unique<web::FakeWebState>();
  FakeWebState* web_state_ptr = web_state.get();

  GURL url(kChromeUINewTabURL);
  web_state_ptr->SetCurrentURL(url);

  web_state_list_->InsertWebState(
      0, std::move(web_state), WebStateList::INSERT_NO_FLAGS, WebStateOpener());

  GURL not_ntp_url(kTestURL);
  auto second_tab = std::make_unique<web::FakeWebState>();
  web::FakeWebState* second_tab_ptr = second_tab.get();
  web_state_list_->InsertWebState(1, std::move(second_tab),
                                  WebStateList::INSERT_NO_FLAGS,
                                  WebStateOpener());

  web::FakeNavigationContext context;
  second_tab_ptr->SetCurrentURL(not_ntp_url);
  second_tab_ptr->OnNavigationFinished(&context);

  testing::Mock::VerifyAndClearExpectations(controller_.get());

  // Now switch to first tab - which is NTP.
  EXPECT_CALL(*controller_, StartPrefetch).Times(1);
  EXPECT_CALL(*controller_, Start).Times(0);
  web_state_list_->ActivateWebStateAt(0);
  web_state_ptr->WasShown();
  testing::Mock::VerifyAndClearExpectations(controller_.get());

  // Now switch to second tab - which isn't NTP.
  EXPECT_CALL(*controller_, StartPrefetch).Times(0);
  EXPECT_CALL(*controller_, Start).Times(0);
  web_state_list_->ActivateWebStateAt(1);
  second_tab_ptr->WasShown();
  testing::Mock::VerifyAndClearExpectations(controller_.get());

  // Back to first.
  EXPECT_CALL(*controller_, StartPrefetch).Times(1);
  EXPECT_CALL(*controller_, Start).Times(0);
  web_state_list_->ActivateWebStateAt(0);
  web_state_ptr->WasShown();
  testing::Mock::VerifyAndClearExpectations(controller_.get());

  // And back to second!
  EXPECT_CALL(*controller_, StartPrefetch).Times(0);
  EXPECT_CALL(*controller_, Start).Times(0);
  web_state_list_->ActivateWebStateAt(1);
  second_tab_ptr->WasShown();
  testing::Mock::VerifyAndClearExpectations(controller_.get());
}

// Test that adding a second NTP in the tab switcher and switching to it starts
// prefetch again.
TEST_F(ZeroSuggestPrefetchHelperTest, TestAddSecondTab) {
  auto web_state = std::make_unique<web::FakeWebState>();
  FakeWebState* web_state_ptr = web_state.get();

  GURL url(kChromeUINewTabURL);
  web_state_ptr->SetCurrentURL(url);

  web_state_list_->InsertWebState(
      0, std::move(web_state), WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  web_state_list_->ActivateWebStateAt(0);

  EXPECT_CALL(*controller_, StartPrefetch).Times(1);
  EXPECT_CALL(*controller_, Start).Times(0);

  CreateHelper();

  testing::Mock::VerifyAndClearExpectations(controller_.get());

  // Now add a new NTP.
  EXPECT_CALL(*controller_, StartPrefetch).Times(1);
  EXPECT_CALL(*controller_, Start).Times(0);

  auto second_tab = std::make_unique<web::FakeWebState>();
  web::FakeWebState* second_tab_ptr = second_tab.get();
  web_state_list_->InsertWebState(1, std::move(second_tab),
                                  WebStateList::INSERT_NO_FLAGS,
                                  WebStateOpener());
  web_state_list_->ActivateWebStateAt(1);

  web::FakeNavigationContext context;
  second_tab_ptr->SetCurrentURL(url);
  second_tab_ptr->OnNavigationFinished(&context);

  testing::Mock::VerifyAndClearExpectations(controller_.get());
}

}  // namespace
