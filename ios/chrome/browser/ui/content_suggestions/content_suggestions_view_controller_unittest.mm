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
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/most_visited_tiles_config.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/shortcuts_config.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_config.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
const char kURL[] = "https://chromium.org/";
}

class ContentSuggestionsViewControllerTest : public PlatformTest {
 public:
  ContentSuggestionsViewControllerTest() {
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
    config.shortcutItems = @[ BookmarkActionItem() ];
    return config;
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
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{segmentation_platform::features::kSegmentationPlatformFeature, {}},
       {segmentation_platform::features::kSegmentationPlatformIosModuleRanker,
        {{segmentation_platform::kDefaultModelEnabledParam, "true"}}},
       {kMagicStack, {}}},
      {});

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

// Tests that modules are inserted in their correct final positions in the Magic
// Stack after initial Magic Stack construction no matter what order the modules
// are made available.
TEST_F(ContentSuggestionsViewControllerTest, TestInsertModuleIntoMagicStack) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kMagicStack, {{kMagicStackMostVisitedModuleParam, "true"}}},
       {kSafetyCheckMagicStack, {}}},
      {});
  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kMostVisited)),
    @(int(ContentSuggestionsModuleType::kShortcuts)),
    @(int(ContentSuggestionsModuleType::kSafetyCheck))
  ]];
  // Simulate scenario where:
  // Shortcuts should be inserted at index 0
  // Safety Check should be inserted at index 1
  // Most Visited should be inserted at index 0
  [view_controller_ setShortcutTilesConfig:ShortcutsConfigWithBookmark()];
  // Trigger -viewDidLoad for initial Magic Stack construction.
  // TODO(crbug.com/1477476): This view get should ideally happen before
  // setShortcutTilesConfig: to ensure Shortcuts is inserted correctly as
  // well.
  [view_controller_ loadViewIfNeeded];
  // If not implemented correctly, based on what is passed in
  // `setMagicStackOrder:` Safety Check could be added at index 2, which would
  // throw an insert out of bounds exception.
  SafetyCheckState* defaultSafetyCheckState = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];
  [view_controller_ showSafetyCheck:defaultSafetyCheckState];
  [view_controller_ setMostVisitedTilesConfig:MVTConfig()];

  UIStackView* magicStack = FindMagicStack();
  // Assert order is correct.
  NSArray<UIView*>* subviews = magicStack.arrangedSubviews;
  // Three modules and edit button.
  ASSERT_EQ(4u, [subviews count]);
  MagicStackModuleContainer* mostVisitedModule =
      (MagicStackModuleContainer*)subviews[0];
  EXPECT_EQ(ContentSuggestionsModuleType::kMostVisited, mostVisitedModule.type);
  MagicStackModuleContainer* shortcutsModule =
      (MagicStackModuleContainer*)subviews[1];
  EXPECT_EQ(ContentSuggestionsModuleType::kShortcuts, shortcutsModule.type);
  MagicStackModuleContainer* safetyCheckModule =
      (MagicStackModuleContainer*)subviews[2];
  EXPECT_EQ(ContentSuggestionsModuleType::kSafetyCheck, safetyCheckModule.type);
}

// Tests that updates to the Magic Stack module order result in the correct
// order of modules in the Magic Stack.
TEST_F(ContentSuggestionsViewControllerTest, TestUpdateMagicStackOrder) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kMagicStack, {{kMagicStackMostVisitedModuleParam, "true"}}},
       {kSafetyCheckMagicStack, {}},
       {kTabResumption, {}}},
      {});
  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kMostVisited)),
    @(int(ContentSuggestionsModuleType::kSafetyCheck)),
    @(int(ContentSuggestionsModuleType::kShortcuts))
  ]];

  [view_controller_ setShortcutTilesConfig:ShortcutsConfigWithBookmark()];
  // Trigger -viewDidLoad for initial Magic Stack construction.
  // TODO(crbug.com/1477476): This view get should ideally happen before
  // setShortcutTilesConfig: to ensure Shortcuts is inserted correctly as
  // well.
  [view_controller_ loadViewIfNeeded];
  SafetyCheckState* defaultSafetyCheckState = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];
  [view_controller_ showSafetyCheck:defaultSafetyCheckState];
  [view_controller_ setMostVisitedTilesConfig:MVTConfig()];

  // Verify Removing kSafetyCheck works.
  MagicStackOrderChange change;
  change.type = MagicStackOrderChange::Type::kRemove;
  change.old_module = ContentSuggestionsModuleType::kSafetyCheck;
  change.index = 1;
  [view_controller_ updateMagicStackOrder:change];
  UIStackView* magicStack = FindMagicStack();
  // Assert order is correct.
  NSArray<UIView*>* subviews = magicStack.arrangedSubviews;
  // Two modules and edit button.
  ASSERT_EQ(3u, [subviews count]);
  MagicStackModuleContainer* mostVisitedModule =
      (MagicStackModuleContainer*)subviews[0];
  EXPECT_EQ(ContentSuggestionsModuleType::kMostVisited, mostVisitedModule.type);
  MagicStackModuleContainer* shortcutsModule =
      (MagicStackModuleContainer*)subviews[1];
  EXPECT_EQ(ContentSuggestionsModuleType::kShortcuts, shortcutsModule.type);

  // Verify Inserting kTabResumption works.
  change.type = MagicStackOrderChange::Type::kInsert;
  change.new_module = ContentSuggestionsModuleType::kTabResumption;
  change.index = 1;
  [view_controller_ updateMagicStackOrder:change];
  TabResumptionItem* item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kMostRecentTab];
  item.tabTitle = @"Some Title";
  item.syncedTime = base::Time::Now();
  item.tabURL = GURL(kURL);
  [view_controller_ showTabResumptionWithItem:item];
  magicStack = FindMagicStack();
  // Assert order is correct.
  subviews = magicStack.arrangedSubviews;
  // Three modules and edit button.
  ASSERT_EQ(4u, [subviews count]);
  mostVisitedModule = (MagicStackModuleContainer*)subviews[0];
  EXPECT_EQ(ContentSuggestionsModuleType::kMostVisited, mostVisitedModule.type);
  shortcutsModule = (MagicStackModuleContainer*)subviews[1];
  EXPECT_EQ(ContentSuggestionsModuleType::kTabResumption, shortcutsModule.type);
  MagicStackModuleContainer* tabResumptionModule =
      (MagicStackModuleContainer*)subviews[2];
  EXPECT_EQ(ContentSuggestionsModuleType::kShortcuts, tabResumptionModule.type);
}

// Tests that the Safety Check module (of type `kSafetyCheck`) correctly
// replaces itself at the index of an existing Safety Check module.
TEST_F(ContentSuggestionsViewControllerTest,
       TestReplaceSafetyCheckMultiRowWithSafetyCheck) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kMagicStack, {{kMagicStackMostVisitedModuleParam, "true"}}},
       {kSafetyCheckMagicStack, {}}},
      {});

  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kMostVisited)),
    @(int(ContentSuggestionsModuleType::kShortcuts)),
    @(int(ContentSuggestionsModuleType::kSafetyCheck)),
  ]];

  [view_controller_ setShortcutTilesConfig:ShortcutsConfigWithBookmark()];

  [view_controller_ loadViewIfNeeded];

  SafetyCheckState* multiRowSafetyCheckState = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kOutOfDate
                  passwordState:PasswordSafetyCheckState::
                                    kUnmutedCompromisedPasswords
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];

  [view_controller_ setMostVisitedTilesConfig:MVTConfig()];

  [view_controller_ showSafetyCheck:multiRowSafetyCheckState];

  UIStackView* magicStack = FindMagicStack();

  // Assert order is correct.
  NSArray<UIView*>* subviews = magicStack.arrangedSubviews;

  // Three modules and edit button.
  ASSERT_EQ(4u, [subviews count]);

  MagicStackModuleContainer* mostVisitedModule =
      (MagicStackModuleContainer*)subviews[0];

  EXPECT_EQ(ContentSuggestionsModuleType::kMostVisited, mostVisitedModule.type);

  MagicStackModuleContainer* shortcutsModule =
      (MagicStackModuleContainer*)subviews[1];

  EXPECT_EQ(ContentSuggestionsModuleType::kShortcuts, shortcutsModule.type);

  MagicStackModuleContainer* safetyCheckModule =
      (MagicStackModuleContainer*)subviews[2];

  EXPECT_EQ(ContentSuggestionsModuleType::kSafetyCheck, safetyCheckModule.type);

  SafetyCheckState* defaultSafetyCheckState = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];

  [view_controller_ showSafetyCheck:defaultSafetyCheckState];

  magicStack = FindMagicStack();

  // Assert order is correct.
  subviews = magicStack.arrangedSubviews;

  // Three modules and edit button.
  ASSERT_EQ(4u, [subviews count]);

  safetyCheckModule = (MagicStackModuleContainer*)subviews[2];

  EXPECT_EQ(ContentSuggestionsModuleType::kSafetyCheck, safetyCheckModule.type);
}

// Test that with Magic Stack and Segmentation enabled, the Magic Stack is
// created upon surface creation with two placeholder modules even without the
// Magic Stack order available yet, and that the placeholders are replaced with
// the real modules once the Magic Stack order is received.
TEST_F(ContentSuggestionsViewControllerTest, TestMagicStackPlaceholder) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{segmentation_platform::features::kSegmentationPlatformFeature, {}},
       {segmentation_platform::features::kSegmentationPlatformIosModuleRanker,
        {{segmentation_platform::kDefaultModelEnabledParam, "true"}}},
       {kMagicStack, {}}},
      {});

  [view_controller_ setShortcutTilesConfig:ShortcutsConfigWithBookmark()];

  [view_controller_ loadViewIfNeeded];

  // Verify that after initial load with no Magic Stack order, two placeholder
  // modules are in the Magic Stack.
  UIStackView* magicStack = FindMagicStack();
  NSArray<UIView*>* subviews = magicStack.arrangedSubviews;
  ASSERT_EQ(2u, [subviews count]);

  // Verify that after passing the Magic Stack order, the actual module is the
  // only subview.
  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kMostVisited)),
    @(int(ContentSuggestionsModuleType::kShortcuts)),
  ]];
  magicStack = FindMagicStack();
  subviews = magicStack.arrangedSubviews;
  // One module and edit button.
  ASSERT_EQ(2u, [subviews count]);
}

// Tests that the Parcel Tracking module is added to the magic stack after the
// latter is initially constructed.
TEST_F(ContentSuggestionsViewControllerTest,
       TestInsertParcelTrackingModuleIntoMagicStack) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kMagicStack, {{kMagicStackMostVisitedModuleParam, "true"}}}}, {});
  [view_controller_ setMagicStackOrder:@[
    @(int(ContentSuggestionsModuleType::kMostVisited)),
    @(int(ContentSuggestionsModuleType::kParcelTracking)),
    @(int(ContentSuggestionsModuleType::kShortcuts))
  ]];
  // Simulate scenario where:
  // Shortcuts should be inserted at index 0
  // Safety Check should be inserted at index 1
  // Most Visited should be inserted at index 0
  [view_controller_ setShortcutTilesConfig:ShortcutsConfigWithBookmark()];
  // Trigger -viewDidLoad for initial Magic Stack construction.
  // TODO(crbug.com/1477476): This view get should ideally happen before
  // setShortcutTilesConfig: to ensure Shortcuts is inserted correctly as
  // well.
  [view_controller_ loadViewIfNeeded];

  [view_controller_ setMostVisitedTilesConfig:MVTConfig()];
  ParcelTrackingItem* item = [[ParcelTrackingItem alloc] init];
  item.estimatedDeliveryTime = base::Time();
  [view_controller_ showParcelTrackingItem:item];

  UIStackView* magicStack = FindMagicStack();

  // Assert order is correct.
  NSArray<UIView*>* subviews = magicStack.arrangedSubviews;

  // Three modules and edit button
  ASSERT_EQ(4u, [subviews count]);

  MagicStackModuleContainer* parcelTrackingModule =
      (MagicStackModuleContainer*)subviews[1];

  EXPECT_EQ(ContentSuggestionsModuleType::kParcelTracking,
            parcelTrackingModule.type);
}
