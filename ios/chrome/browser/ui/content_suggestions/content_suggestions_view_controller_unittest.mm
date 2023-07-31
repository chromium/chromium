// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ContentSuggestionsViewControllerTest : public PlatformTest {
 public:
  ContentSuggestionsViewControllerTest() {
    view_controller_ = [[ContentSuggestionsViewController alloc] init];
    metrics_recorder_ = [[ContentSuggestionsMetricsRecorder alloc]
        initWithLocalState:&pref_service_];
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, -1);
    view_controller_.contentSuggestionsMetricsRecorder = metrics_recorder_;
    histogram_tester_.reset(new base::HistogramTester());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  ContentSuggestionsViewController* view_controller_;
  id metrics_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests that the correct Magic Stack impression metrics are logged depending on
// the Magic Stack order.
TEST_F(ContentSuggestionsViewControllerTest,
       TestMagicStackTopImpressionMetric) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures({kMagicStack, kIOSSetUpList}, {});
  histogram_tester_->ExpectBucketCount(
      kMagicStackTopModuleImpressionHistogram,
      ContentSuggestionsModuleType::kMostVisited, 0);
  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kMostVisited))
  ]];
  [view_controller_ setMostVisitedTilesWithConfigs:@[
    [[ContentSuggestionsMostVisitedItem alloc] init]
  ]];
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
  [view_controller_ setMostVisitedTilesWithConfigs:@[
    [[ContentSuggestionsMostVisitedItem alloc] init]
  ]];
  [view_controller_ setShortcutTilesWithConfigs:@[ BookmarkActionItem() ]];
  histogram_tester_->ExpectBucketCount(kMagicStackTopModuleImpressionHistogram,
                                       ContentSuggestionsModuleType::kShortcuts,
                                       1);

  FirstRun::RemoveSentinel();
  base::File::Error fileError;
  FirstRun::CreateSentinel(&fileError);
  FirstRun::LoadSentinelInfo();

  histogram_tester_->ExpectBucketCount(
      kMagicStackTopModuleImpressionHistogram,
      ContentSuggestionsModuleType::kSetUpListSync, 0);
  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kSetUpListSync)),
    @(int(ContentSuggestionsModuleType::kSetUpListDefaultBrowser)),
    @(int(ContentSuggestionsModuleType::kSetUpListAutofill)),
    @(int(ContentSuggestionsModuleType::kShortcuts))
  ]];
  [view_controller_ showSetUpListWithItems:@[
    [[SetUpListItemViewData alloc] initWithType:SetUpListItemType::kSignInSync
                                       complete:NO],
    [[SetUpListItemViewData alloc]
        initWithType:SetUpListItemType::kDefaultBrowser
            complete:NO],
    [[SetUpListItemViewData alloc] initWithType:SetUpListItemType::kAutofill
                                       complete:NO]
  ]];
  [view_controller_ setShortcutTilesWithConfigs:@[ BookmarkActionItem() ]];
  [view_controller_ view];
  histogram_tester_->ExpectBucketCount(
      kMagicStackTopModuleImpressionHistogram,
      ContentSuggestionsModuleType::kSetUpListSync, 1);
}
