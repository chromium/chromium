// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_collection_view.h"

#import "base/test/scoped_feature_list.h"
#import "components/segmentation_platform/public/features.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_config.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_collection_view_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface MagicStackCollectionViewController (Testing)

- (CGFloat)getNextPageOffsetForOffset:(CGFloat)offset
                             velocity:(CGFloat)velocity;

@end

// Tests the behavior of MagicStackCollectionViewController.
class MagicStackCollectionViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    _window = [[UIWindow alloc] init];
    UIView.animationsEnabled = NO;
    view_controller_ = [[MagicStackCollectionViewController alloc] init];
    audience_ = OCMStrictProtocolMock(
        @protocol(MagicStackCollectionViewControllerAudience));
    view_controller_.audience = audience_;
    [view_controller_ viewDidLoad];
    [_window addSubview:[view_controller_ view]];
    AddSameConstraints(_window, [view_controller_ view]);
    [[view_controller_ view] layoutIfNeeded];

    pref_service_.registry()->RegisterIntegerPref(
        prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness,
        -1);

    TestingApplicationContext::GetGlobal()->SetLocalState(&pref_service_);
  }

  void TearDown() override {
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  UIWindow* _window;
  UIView* _superview;
  MagicStackCollectionViewController* view_controller_;
  id<MagicStackCollectionViewControllerAudience,
     MagicStackModuleContainerDelegate>
      audience_;
};

// Tests that bringing an ephemeral card into view triggers the expected
// audience signal.
TEST_F(MagicStackCollectionViewControllerTest, TestEphemeralCardAudienceCall) {
  scoped_feature_list_.InitWithFeatures(
      {segmentation_platform::features::
           kSegmentationPlatformEphemeralCardRanker},
      {});
  OCMExpect([audience_ logEphemeralCardVisibility:ContentSuggestionsModuleType::
                                                      kPriceTrackingPromo]);
  // Test that populating the Magic Stack triggers audience call
  [view_controller_ populateItems:@[
    [[PriceTrackingPromoItem alloc] init], [[ShortcutsConfig alloc] init]
  ]];
  EXPECT_OCMOCK_VERIFY((id)audience_);

  // Test that the audience call is not triggered more than once
  [view_controller_ getNextPageOffsetForOffset:0 velocity:0];
  EXPECT_OCMOCK_VERIFY((id)audience_);
}

// Tests that swiping to an ephemeral card when it is not the top card triggers
// the expected audience signal.
TEST_F(MagicStackCollectionViewControllerTest,
       TestSwipeToEphemeralCardAudienceCall) {
  scoped_feature_list_.InitWithFeatures(
      {segmentation_platform::features::
           kSegmentationPlatformEphemeralCardRanker},
      {});
  // Test that populating the Magic Stack does not trigger audience call since
  // it is not top card.
  [view_controller_ populateItems:@[
    [[ShortcutsConfig alloc] init], [[PriceTrackingPromoItem alloc] init]
  ]];
  EXPECT_OCMOCK_VERIFY((id)audience_);

  OCMExpect([audience_ logEphemeralCardVisibility:ContentSuggestionsModuleType::
                                                      kPriceTrackingPromo]);
  // Test that scrolling to card triggers audience signal.
  [view_controller_ getNextPageOffsetForOffset:400 velocity:.1f];
  EXPECT_OCMOCK_VERIFY((id)audience_);
}
