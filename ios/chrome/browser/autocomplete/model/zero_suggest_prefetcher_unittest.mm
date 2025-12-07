// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/zero_suggest_prefetcher.h"

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/mock_autocomplete_provider_client.h"
#import "components/omnibox/browser/test_scheme_classifier.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController(
      std::unique_ptr<AutocompleteProviderClient> provider_client)
      : AutocompleteController(std::move(provider_client),
                               AutocompleteControllerConfig()) {}
  MOCK_METHOD(void, StartPrefetch, (const AutocompleteInput&), (override));
};

class ZeroSuggestPrefetcherTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    auto mock_client = std::make_unique<MockAutocompleteProviderClient>();
    mock_client_ptr_ = mock_client.get();
    ON_CALL(*mock_client_ptr_, GetSchemeClassifier())
        .WillByDefault(testing::ReturnRef(scheme_classifier_));
    autocomplete_controller_ =
        std::make_unique<MockAutocompleteController>(std::move(mock_client));
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
  }

  void CreatePrefetcherWithWebStateList() {
    prefetcher_ = [[ZeroSuggestPrefetcher alloc]
        initWithAutocompleteController:autocomplete_controller_.get()
                          webStateList:web_state_list_.get()
                classificationCallback:base::BindRepeating([]() {
                  return metrics::OmniboxEventProto::OTHER;
                })
                    disconnectCallback:base::BindOnce(
                                           &ZeroSuggestPrefetcherTest::
                                               OnDisconnectWebStateList,
                                           base::Unretained(this))];
  }

  void CreatePrefetcherWithWebState(web::WebState* web_state) {
    prefetcher_ = [[ZeroSuggestPrefetcher alloc]
        initWithAutocompleteController:autocomplete_controller_.get()
                              webState:web_state
                classificationCallback:base::BindRepeating([]() {
                  return metrics::OmniboxEventProto::OTHER;
                })
                    disconnectCallback:
                        base::BindOnce(
                            &ZeroSuggestPrefetcherTest::OnDisconnectWebState,
                            base::Unretained(this))];
  }

  void OnDisconnectWebStateList(WebStateList* web_state_list) {
    web_state_list_disconnected_ = true;
  }

  void OnDisconnectWebState(web::WebState* web_state) {
    web_state_disconnected_ = true;
  }

  base::test::TaskEnvironment task_environment_;
  TestSchemeClassifier scheme_classifier_;
  std::unique_ptr<MockAutocompleteController> autocomplete_controller_;
  raw_ptr<MockAutocompleteProviderClient> mock_client_ptr_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  ZeroSuggestPrefetcher* prefetcher_;
  bool web_state_list_disconnected_ = false;
  bool web_state_disconnected_ = false;
};

// Tests that the prefetcher reacts correctly to app foregrounding and
// backgrounding.
TEST_F(ZeroSuggestPrefetcherTest, TestReactToForegroundingAndBackgrounding) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(GURL("https://example.com"));
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(0);

  EXPECT_CALL(*autocomplete_controller_, StartPrefetch(testing::_)).Times(1);
  CreatePrefetcherWithWebStateList();

  // Test backgrounding
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];
  EXPECT_TRUE(mock_client_ptr_->in_background_state());

  // Test foregrounding
  EXPECT_CALL(*autocomplete_controller_, StartPrefetch(testing::_)).Times(1);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillEnterForegroundNotification
                    object:nil];
  EXPECT_FALSE(mock_client_ptr_->in_background_state());
}

// Tests that the prefetcher triggers a prefetch when the active tab changes.
TEST_F(ZeroSuggestPrefetcherTest, TestPrefetchOnTabSwitch) {
  auto web_state1 = std::make_unique<web::FakeWebState>();
  web_state1->SetVisibleURL(GURL("https://example.com/1"));
  web_state_list_->InsertWebState(std::move(web_state1));

  auto web_state2 = std::make_unique<web::FakeWebState>();
  web_state2->SetVisibleURL(GURL("https://example.com/2"));
  web_state_list_->InsertWebState(std::move(web_state2));

  web_state_list_->ActivateWebStateAt(0);

  EXPECT_CALL(*autocomplete_controller_, StartPrefetch(testing::_)).Times(1);
  CreatePrefetcherWithWebStateList();

  // Switch to tab 2
  EXPECT_CALL(*autocomplete_controller_, StartPrefetch(testing::_)).Times(1);
  web_state_list_->ActivateWebStateAt(1);
}

// Tests that the prefetcher triggers a prefetch when a navigation finishes.
TEST_F(ZeroSuggestPrefetcherTest, TestReactToNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(GURL("https://example.com"));
  web::FakeWebState* web_state_ptr = web_state.get();
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(0);

  EXPECT_CALL(*autocomplete_controller_, StartPrefetch(testing::_)).Times(1);
  CreatePrefetcherWithWebStateList();

  // Simulate navigation
  EXPECT_CALL(*autocomplete_controller_, StartPrefetch(testing::_)).Times(1);
  web_state_ptr->OnNavigationFinished(nullptr);
}

// Tests that the prefetcher cleans up correctly when the observed WebState is
// destroyed.
TEST_F(ZeroSuggestPrefetcherTest, TestCleanupOnWebStateDestroyed) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(GURL("https://example.com"));
  web::FakeWebState* web_state_ptr = web_state.get();

  EXPECT_CALL(*autocomplete_controller_, StartPrefetch(testing::_)).Times(1);
  CreatePrefetcherWithWebState(web_state_ptr);

  // Destroy web state
  web_state.reset();
  EXPECT_TRUE(web_state_disconnected_);
}

// Tests that the prefetcher cleans up correctly when the observed WebStateList
// is destroyed.
TEST_F(ZeroSuggestPrefetcherTest, TestCleanupOnWebStateListDestroyed) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(GURL("https://example.com"));
  web_state_list_->InsertWebState(std::move(web_state));
  web_state_list_->ActivateWebStateAt(0);

  EXPECT_CALL(*autocomplete_controller_, StartPrefetch(testing::_)).Times(1);
  CreatePrefetcherWithWebStateList();

  // Destroy web state list
  web_state_list_.reset();
  EXPECT_TRUE(web_state_list_disconnected_);
}

}  // namespace
