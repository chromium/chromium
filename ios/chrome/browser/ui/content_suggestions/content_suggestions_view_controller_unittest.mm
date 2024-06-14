// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_config.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_config.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

class ContentSuggestionsViewControllerTest : public PlatformTest {
 public:
  ContentSuggestionsViewControllerTest() {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());

    view_controller_ = [[ContentSuggestionsViewController alloc] init];
    metrics_recorder_ = [[ContentSuggestionsMetricsRecorder alloc]
        initWithLocalState:&pref_service_];
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, -1);
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness,
        -1);
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
        -1);
    view_controller_.contentSuggestionsMetricsRecorder = metrics_recorder_;
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
  }

  // Iterates a view's subviews recursively, calling the block with each one.
  bool IterateSubviews(UIView* view, bool (^block)(UIView* subview)) {
    for (UIView* subview in view.subviews) {
      if (block(subview) || IterateSubviews(subview, block)) {
        return true;
      }
    }
    return false;
  }

  UIStackView* FindMagicStack() {
    __block UIStackView* found = nil;
    IterateSubviews(view_controller_.view, ^bool(UIView* subview) {
      if ([subview.accessibilityIdentifier
              isEqual:kMagicStackViewAccessibilityIdentifier]) {
        found = (UIStackView*)subview;
        return true;
      }
      return false;
    });
    return found;
  }

  MostVisitedTilesConfig* MVTConfig() {
    MostVisitedTilesConfig* mvtConfig = [[MostVisitedTilesConfig alloc] init];
    mvtConfig.mostVisitedItems =
        @[ [[ContentSuggestionsMostVisitedItem alloc] init] ];
    return mvtConfig;
  }

  ShortcutsConfig* ShortcutsConfigWithBookmark() {
    ShortcutsConfig* config = [[ShortcutsConfig alloc] init];
    config.shortcutItems = @[ [[ContentSuggestionsMostVisitedActionItem alloc]
        initWithCollectionShortcutType:NTPCollectionShortcutTypeBookmark] ];
    return config;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  ContentSuggestionsViewController* view_controller_;
  id metrics_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
};

// Tests that the correct Magic Stack impression metrics are logged depending on
// the Magic Stack order.
TEST_F(ContentSuggestionsViewControllerTest,
       TestMagicStackTopImpressionMetric) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures({kMagicStack}, {});
  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kMostVisited))
  ]];
  [view_controller_ loadViewIfNeeded];
  histogram_tester_->ExpectBucketCount(
      kMagicStackTopModuleImpressionHistogram,
      ContentSuggestionsModuleType::kMostVisited, 0);
  [view_controller_ setMostVisitedTilesConfig:MVTConfig()];
  histogram_tester_->ExpectBucketCount(
      kMagicStackTopModuleImpressionHistogram,
      ContentSuggestionsModuleType::kMostVisited, 1);

  histogram_tester_->ExpectBucketCount(kMagicStackTopModuleImpressionHistogram,
                                       ContentSuggestionsModuleType::kShortcuts,
                                       0);
  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kShortcuts)),
    @(int(ContentSuggestionsModuleType::kMostVisited))
  ]];
  [view_controller_ setMostVisitedTilesConfig:MVTConfig()];
  [view_controller_ setShortcutTilesConfig:ShortcutsConfigWithBookmark()];
  histogram_tester_->ExpectBucketCount(kMagicStackTopModuleImpressionHistogram,
                                       ContentSuggestionsModuleType::kShortcuts,
                                       1);

  FirstRun::RemoveSentinel();
  base::File::Error fileError;
  FirstRun::CreateSentinel(&fileError);
  FirstRun::LoadSentinelInfo();

  histogram_tester_->ExpectBucketCount(
      kMagicStackTopModuleImpressionHistogram,
      ContentSuggestionsModuleType::kCompactedSetUpList, 0);
  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kCompactedSetUpList)),
    @(int(ContentSuggestionsModuleType::kShortcuts))
  ]];
  SetUpListConfig* config = [[SetUpListConfig alloc] init];
  config.shouldShowCompactModule = YES;
  config.setUpListItems = @[
    [[SetUpListItemViewData alloc] initWithType:SetUpListItemType::kSignInSync
                                       complete:NO],
    [[SetUpListItemViewData alloc]
        initWithType:SetUpListItemType::kDefaultBrowser
            complete:NO],
    [[SetUpListItemViewData alloc] initWithType:SetUpListItemType::kAutofill
                                       complete:NO]
  ];
  [view_controller_ showSetUpListModuleWithConfigs:@[ config ]];
  [view_controller_ setShortcutTilesConfig:ShortcutsConfigWithBookmark()];
  [view_controller_ view];
  histogram_tester_->ExpectBucketCount(
      kMagicStackTopModuleImpressionHistogram,
      ContentSuggestionsModuleType::kCompactedSetUpList, 1);
}

// Tests that the Magic Stack top module impression metric logs correctly even
// if the Magic Stack module rank is ready after the top module is. This
// simulates an environment where kSegmentationPlatformFeature is enabled, so
// the magic stack module rank is asynchonously fetched and could be available
// only after the initial view construction.
TEST_F(ContentSuggestionsViewControllerTest,
       TestMagicStackTopImpressionMetricSegmentation) {
  [view_controller_ setShortcutTilesConfig:ShortcutsConfigWithBookmark()];
  histogram_tester_->ExpectBucketCount(kMagicStackTopModuleImpressionHistogram,
                                       ContentSuggestionsModuleType::kShortcuts,
                                       0);
  [view_controller_ loadViewIfNeeded];
  histogram_tester_->ExpectBucketCount(kMagicStackTopModuleImpressionHistogram,
                                       ContentSuggestionsModuleType::kShortcuts,
                                       0);
  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kShortcuts)),
    @(int(ContentSuggestionsModuleType::kMostVisited))
  ]];
  histogram_tester_->ExpectBucketCount(kMagicStackTopModuleImpressionHistogram,
                                       ContentSuggestionsModuleType::kShortcuts,
                                       1);
}
