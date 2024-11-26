// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser//ui/content_suggestions/tips/tips_magic_stack_mediator.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/test/test_bookmark_client.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_prefs.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using segmentation_platform::TipIdentifier;

// Tests the `TipsMagicStackMediator`.
class TipsMagicStackMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    tips_prefs::RegisterPrefs(profile_pref_service_.registry());

    // Create a `TipsMagicStackMediator` with an initial unknown
    // `TipIdentifier`.
    mediator_ = [[TipsMagicStackMediator alloc]
        initWithIdentifier:TipIdentifier::kUnknown
        profilePrefService:&profile_pref_service_
           shoppingService:shopping_service_.get()
             bookmarkModel:bookmark_model_.get()
              imageFetcher:std::make_unique<image_fetcher::ImageDataFetcher>(
                               profile_->GetSharedURLLoaderFactory())];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TipsMagicStackMediator* mediator_;
  std::unique_ptr<TestProfileIOS> profile_;
  sync_preferences::TestingPrefServiceSyncable profile_pref_service_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

// Tests that the mediator's initial state is configured correctly for an
// unknown tip.
TEST_F(TipsMagicStackMediatorTest, HasCorrectInitialStateForUnknownTip) {
  EXPECT_EQ(TipIdentifier::kUnknown, mediator_.state.identifier);
}

// Tests that the mediator reconfigures its state to reflect a new tip.
TEST_F(TipsMagicStackMediatorTest, ReconfiguresStateForNewTip) {
  [mediator_ reconfigureWithTipIdentifier:TipIdentifier::kLensShop];

  EXPECT_EQ(TipIdentifier::kLensShop, mediator_.state.identifier);
}

// Tests that the mediator calls `-removeTipsModule` on its delegate when
// disabled.
TEST_F(TipsMagicStackMediatorTest, CallsRemoveModuleOnDelegate) {
  id delegate =
      OCMStrictProtocolMock(@protocol(TipsMagicStackMediatorDelegate));
  mediator_.delegate = delegate;

  // Set up the expectation for the delegate method call, using the
  // completionBlock.
  OCMExpect([delegate removeTipsModuleWithCompletion:[OCMArg any]]);

  [mediator_ disableModule];

  // Verify that the delegate method was called.
  EXPECT_OCMOCK_VERIFY(delegate);
}
