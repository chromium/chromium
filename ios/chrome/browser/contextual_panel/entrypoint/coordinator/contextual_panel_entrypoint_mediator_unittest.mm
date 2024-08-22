// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/scoped_iph_feature_list.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// A fake ContextualPanelEntrypointConsumer for use in tests.
@interface FakeEntrypointConsumer : NSObject <ContextualPanelEntrypointConsumer>

@property(nonatomic, assign) BOOL entrypointIsShown;

@property(nonatomic, assign) BOOL entrypointIsLarge;

@property(nonatomic, assign) BOOL contextualPanelIsOpen;

@property(nonatomic, assign) BOOL entrypointIsColored;

@property(nonatomic, assign) base::WeakPtr<ContextualPanelItemConfiguration>
    currentConfiguration;

@end

@implementation FakeEntrypointConsumer

- (void)setEntrypointConfig:
    (base::WeakPtr<ContextualPanelItemConfiguration>)config {
  self.currentConfiguration = config;
}

- (void)hideEntrypoint {
  self.entrypointIsShown = NO;
}

- (void)showEntrypoint {
  self.entrypointIsShown = YES;
}

- (void)transitionToLargeEntrypoint {
  self.entrypointIsLarge = YES;
}

- (void)transitionToSmallEntrypoint {
  self.entrypointIsLarge = NO;
}

- (void)transitionToContextualPanelOpenedState:(BOOL)opened {
  self.contextualPanelIsOpen = opened;
}

- (void)setInfobarBadgesCurrentlyShown:(BOOL)infobarBadgesCurrentlyShown {
}

- (void)setEntrypointColored:(BOOL)colored {
  self.entrypointIsColored = colored;
}

@end

// Fake test implementation of ContextualPanelEntrypointMediatorDelegate
@interface FakeContextualPanelEntrypointMediatorDelegate
    : NSObject <ContextualPanelEntrypointMediatorDelegate>

@property(nonatomic, assign) BOOL canShowLargeContextualPanelEntrypoint;

@end

@implementation FakeContextualPanelEntrypointMediatorDelegate

- (BOOL)canShowLargeContextualPanelEntrypoint:
    (ContextualPanelEntrypointMediator*)mediator {
  return self.canShowLargeContextualPanelEntrypoint;
}

- (void)setLocationBarLabelCenteredBetweenContent:
            (ContextualPanelEntrypointMediator*)mediator
                                         centered:(BOOL)centered {
  // No-op.
}

- (void)disableFullscreen {
  // No-op.
}

- (void)enableFullscreen {
  // No-op.
}

- (CGPoint)helpAnchorUsingBottomOmnibox:(BOOL)isBottomOmnibox {
  return CGPointMake(0, 0);
}

- (BOOL)isBottomOmniboxActive {
  return NO;
}

@end

// Test fake to allow easier triggering of ContextualPanelTabHelperObserver
// methods.
class FakeContextualPanelTabHelper : public ContextualPanelTabHelper {
 public:
  explicit FakeContextualPanelTabHelper(
      web::WebState* web_state,
      std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models)
      : ContextualPanelTabHelper(web_state, models) {}

  static void CreateForWebState(
      web::WebState* web_state,
      std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models) {
    web_state->SetUserData(
        UserDataKey(),
        std::make_unique<FakeContextualPanelTabHelper>(web_state, models));
  }

  void AddObserver(ContextualPanelTabHelperObserver* observer) override {
    ContextualPanelTabHelper::AddObserver(observer);
    observers_.AddObserver(observer);
  }
  void RemoveObserver(ContextualPanelTabHelperObserver* observer) override {
    ContextualPanelTabHelper::RemoveObserver(observer);
    observers_.RemoveObserver(observer);
  }

  void CallContextualPanelTabHelperDestroyed() {
    for (auto& observer : observers_) {
      observer.ContextualPanelTabHelperDestroyed(this);
    }
  }

  void CallContextualPanelHasNewData(
      std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
          item_configurations) {
    for (auto& observer : observers_) {
      observer.ContextualPanelHasNewData(this, item_configurations);
    }
  }

  base::WeakPtr<ContextualPanelItemConfiguration> GetFirstCachedConfig()
      override {
    return !configs_.empty() ? configs_[0]->weak_ptr_factory.GetWeakPtr()
                             : nullptr;
  }

  // Helper to add configs to the front of the Fake tab helper cached
  // `configs_`.
  void AddToCachedConfigs(
      std::unique_ptr<SamplePanelItemConfiguration> configuration) {
    configs_.insert(configs_.begin(), std::move(configuration));
  }

  base::ObserverList<ContextualPanelTabHelperObserver, true> observers_;
  std::vector<std::unique_ptr<ContextualPanelItemConfiguration>> configs_;
};

class ContextualPanelEntrypointMediatorTest : public PlatformTest {
 public:
  ContextualPanelEntrypointMediatorTest()
      : web_state_list_(&web_state_list_delegate_) {
    auto web_state = std::make_unique<web::FakeWebState>();
    std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models;
    FakeContextualPanelTabHelper::CreateForWebState(web_state.get(), models);
    InfoBarManagerImpl::CreateForWebState(web_state.get());
    InfobarBadgeTabHelper::GetOrCreateForWebState(web_state.get());
    web_state_list_.InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));

    mocked_entrypoint_help_handler_ =
        OCMStrictProtocolMock(@protocol(ContextualPanelEntrypointIPHCommands));
    mocked_contextual_sheet_handler_ =
        OCMStrictProtocolMock(@protocol(ContextualSheetCommands));

    feature_engagement::test::ScopedIphFeatureList list;
    list.InitAndEnableFeatures(
        {feature_engagement::kIPHiOSContextualPanelSampleModelFeature});

    tracker_ = feature_engagement::CreateTestTracker();

    // Make sure tracker is initialized.
    tracker_->AddOnInitializedCallback(BoolArgumentQuitClosure());
    run_loop_.Run();

    mediator_ = [[ContextualPanelEntrypointMediator alloc]
          initWithWebStateList:&web_state_list_
             engagementTracker:tracker_.get()
        contextualSheetHandler:mocked_contextual_sheet_handler_
         entrypointHelpHandler:mocked_entrypoint_help_handler_];

    entrypoint_consumer_ = [[FakeEntrypointConsumer alloc] init];
    mediator_.consumer = entrypoint_consumer_;

    delegate_ = [[FakeContextualPanelEntrypointMediatorDelegate alloc] init];
    mediator_.delegate = delegate_;
  }

 protected:
  base::RepeatingCallback<void(bool)> BoolArgumentQuitClosure() {
    return base::IgnoreArgs<bool>(run_loop_.QuitClosure());
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::RunLoop run_loop_;
  std::unique_ptr<feature_engagement::Tracker> tracker_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  ContextualPanelEntrypointMediator* mediator_;
  FakeEntrypointConsumer* entrypoint_consumer_;
  FakeContextualPanelEntrypointMediatorDelegate* delegate_;
  id mocked_contextual_sheet_handler_;
  id mocked_entrypoint_help_handler_;
};

// Tests that tapping the entrypoint opens the panel if it's closed and vice
// versa.
TEST_F(ContextualPanelEntrypointMediatorTest, TestEntrypointTapped) {
  const base::HistogramTester histogram_tester;
  ContextualPanelTabHelper* tab_helper = ContextualPanelTabHelper::FromWebState(
      web_state_list_.GetActiveWebState());

  // Set the metrics data for the current entrypoint appearing.
  ContextualPanelTabHelper::EntrypointMetricsData metrics_data;
  metrics_data.entrypoint_item_type = ContextualPanelItemType::SamplePanelItem;
  metrics_data.appearance_time = base::Time::Now() - base::Seconds(10);
  tab_helper->SetMetricsData(metrics_data);

  [[mocked_contextual_sheet_handler_ expect] openContextualSheet];
  [[mocked_entrypoint_help_handler_ expect]
      dismissContextualPanelEntrypointIPHAnimated:YES];

  [mediator_ entrypointTapped];
  tab_helper->OpenContextualPanel();
  EXPECT_TRUE(entrypoint_consumer_.contextualPanelIsOpen);

  [[mocked_contextual_sheet_handler_ expect] closeContextualSheet];
  [[mocked_entrypoint_help_handler_ expect]
      dismissContextualPanelEntrypointIPHAnimated:YES];

  [mediator_ entrypointTapped];
  tab_helper->CloseContextualPanel();
  EXPECT_FALSE(entrypoint_consumer_.contextualPanelIsOpen);

  [mocked_contextual_sheet_handler_ verify];
  [mocked_entrypoint_help_handler_ verify];

  histogram_tester.ExpectUniqueSample("IOS.ContextualPanel.Entrypoint.Regular",
                                      EntrypointInteractionType::Tapped, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.ContextualPanel.Entrypoint.Regular.SamplePanelItem",
      EntrypointInteractionType::Tapped, 1);

  histogram_tester.ExpectUniqueSample("IOS.ContextualPanel.EntrypointTapped",
                                      ContextualPanelItemType::SamplePanelItem,
                                      1);

  histogram_tester.ExpectTimeBucketCount(
      "IOS.ContextualPanel.Entrypoint.Regular.UptimeBeforeTap",
      base::Seconds(10), 1);
  histogram_tester.ExpectTimeBucketCount(
      "IOS.ContextualPanel.Entrypoint.Regular.SamplePanelItem.UptimeBeforeTap",
      base::Seconds(10), 1);
}

TEST_F(ContextualPanelEntrypointMediatorTest, TestTabHelperDestroyed) {
  [[mocked_entrypoint_help_handler_ expect]
      dismissContextualPanelEntrypointIPHAnimated:NO];

  // Start off with entrypoint showing.
  [entrypoint_consumer_ showEntrypoint];

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_.GetActiveWebState()));
  tab_helper->CallContextualPanelTabHelperDestroyed();

  EXPECT_FALSE(entrypoint_consumer_.entrypointIsShown);
  [mocked_entrypoint_help_handler_ verify];
}

// Tests that if one configuration is provided, the entrypoint becomes shown.
TEST_F(ContextualPanelEntrypointMediatorTest, TestOneConfiguration) {
  const base::HistogramTester histogram_tester;
  [[mocked_entrypoint_help_handler_ expect]
      dismissContextualPanelEntrypointIPHAnimated:NO];

  ContextualPanelItemConfiguration configuration(
      ContextualPanelItemType::SamplePanelItem);

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_.GetActiveWebState()));

  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
      item_configurations;
  item_configurations.push_back(configuration.weak_ptr_factory.GetWeakPtr());

  tab_helper->CallContextualPanelHasNewData(item_configurations);

  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsLarge);

  ASSERT_TRUE(entrypoint_consumer_.currentConfiguration);
  EXPECT_EQ(&configuration, entrypoint_consumer_.currentConfiguration.get());

  [mocked_entrypoint_help_handler_ verify];

  histogram_tester.ExpectUniqueSample("IOS.ContextualPanel.EntrypointDisplayed",
                                      ContextualPanelItemType::SamplePanelItem,
                                      1);

  histogram_tester.ExpectUniqueSample("IOS.ContextualPanel.Entrypoint.Regular",
                                      EntrypointInteractionType::Displayed, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.ContextualPanel.Entrypoint.Regular.SamplePanelItem",
      EntrypointInteractionType::Displayed, 1);
}

// Tests that -disconnect doesn't crash and that nothing is observing the tab
// helper after disconnecting.s
TEST_F(ContextualPanelEntrypointMediatorTest, TestDisconnect) {
  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_.GetActiveWebState()));

  EXPECT_FALSE(tab_helper->observers_.empty());
  [mediator_ disconnect];
  EXPECT_TRUE(tab_helper->observers_.empty());
}

TEST_F(ContextualPanelEntrypointMediatorTest, TestLargeEntrypointAppears) {
  const base::HistogramTester histogram_tester;
  [[mocked_entrypoint_help_handler_ expect]
      dismissContextualPanelEntrypointIPHAnimated:NO];

  std::unique_ptr<SamplePanelItemConfiguration> configuration =
      std::make_unique<SamplePanelItemConfiguration>();
  configuration->relevance = ContextualPanelItemConfiguration::high_relevance;
  configuration->entrypoint_message = "test";

  delegate_.canShowLargeContextualPanelEntrypoint = YES;

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_.GetActiveWebState()));

  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
      item_configurations;
  item_configurations.push_back(configuration->weak_ptr_factory.GetWeakPtr());
  tab_helper->AddToCachedConfigs(std::move(configuration));

  tab_helper->CallContextualPanelHasNewData(item_configurations);

  // At first, the small entrypoint should be displayed.
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsLarge);

  // Advance time so that the large entrypoint is displayed.
  task_environment_.FastForwardBy(
      base::Seconds(LargeContextualPanelEntrypointDelayInSeconds()));
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsLarge);

  // Advance time until the large entrypoint transitions back to small.
  task_environment_.FastForwardBy(
      base::Seconds(LargeContextualPanelEntrypointDisplayedInSeconds()));
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsLarge);

  [mocked_entrypoint_help_handler_ verify];

  histogram_tester.ExpectUniqueSample("IOS.ContextualPanel.EntrypointDisplayed",
                                      ContextualPanelItemType::SamplePanelItem,
                                      1);

  histogram_tester.ExpectUniqueSample("IOS.ContextualPanel.Entrypoint.Regular",
                                      EntrypointInteractionType::Displayed, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.ContextualPanel.Entrypoint.Regular.SamplePanelItem",
      EntrypointInteractionType::Displayed, 1);

  histogram_tester.ExpectUniqueSample("IOS.ContextualPanel.Entrypoint.Large",
                                      EntrypointInteractionType::Displayed, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.ContextualPanel.Entrypoint.Large.SamplePanelItem",
      EntrypointInteractionType::Displayed, 1);
}

TEST_F(ContextualPanelEntrypointMediatorTest, TestIPHEntrypointAppears) {
  const base::HistogramTester histogram_tester;
  std::unique_ptr<SamplePanelItemConfiguration> configuration =
      std::make_unique<SamplePanelItemConfiguration>();
  configuration->relevance = ContextualPanelItemConfiguration::high_relevance;
  configuration->entrypoint_message = "test";
  configuration->iph_entrypoint_used_event_name = "testUsedEvent";
  configuration->iph_entrypoint_explicitly_dismissed =
      "testExplicitlyDismissedEvent";
  configuration->iph_feature =
      &feature_engagement::kIPHiOSContextualPanelSampleModelFeature;
  configuration->iph_text = "test_text";
  configuration->iph_title = "test_title";
  configuration->iph_image_name = "test_image";

  auto weak_config = configuration->weak_ptr_factory.GetWeakPtr();

  OCMStub([mocked_entrypoint_help_handler_
              maybeShowContextualPanelEntrypointIPHWithConfig:weak_config
                                                  anchorPoint:CGPointMake(0, 0)
                                              isBottomOmnibox:NO])
      .andReturn(YES);

  [[mocked_entrypoint_help_handler_ expect]
      dismissContextualPanelEntrypointIPHAnimated:NO];

  delegate_.canShowLargeContextualPanelEntrypoint = YES;

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_.GetActiveWebState()));

  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
      item_configurations;
  item_configurations.push_back(configuration->weak_ptr_factory.GetWeakPtr());
  tab_helper->AddToCachedConfigs(std::move(configuration));

  tab_helper->CallContextualPanelHasNewData(item_configurations);

  // At first, the small entrypoint should be displayed.
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsLarge);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsColored);

  // Advance time so that the IPH entrypoint is displayed.
  task_environment_.FastForwardBy(
      base::Seconds(LargeContextualPanelEntrypointDelayInSeconds()));
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsLarge);
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsColored);

  [[mocked_entrypoint_help_handler_ expect]
      dismissContextualPanelEntrypointIPHAnimated:YES];

  // Advance time until the IPH is dismissed.
  task_environment_.FastForwardBy(
      base::Seconds(LargeContextualPanelEntrypointDisplayedInSeconds()));
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsLarge);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsColored);

  [mocked_entrypoint_help_handler_ verify];

  histogram_tester.ExpectUniqueSample("IOS.ContextualPanel.EntrypointDisplayed",
                                      ContextualPanelItemType::SamplePanelItem,
                                      1);

  histogram_tester.ExpectUniqueSample("IOS.ContextualPanel.Entrypoint.Regular",
                                      EntrypointInteractionType::Displayed, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.ContextualPanel.Entrypoint.Regular.SamplePanelItem",
      EntrypointInteractionType::Displayed, 1);

  histogram_tester.ExpectUniqueSample("IOS.ContextualPanel.Entrypoint.IPH",
                                      EntrypointInteractionType::Displayed, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.ContextualPanel.Entrypoint.IPH.SamplePanelItem",
      EntrypointInteractionType::Displayed, 1);
}

// Tests a change in the active WebState.
TEST_F(ContextualPanelEntrypointMediatorTest, TestWebStateListChanged) {
  [[mocked_entrypoint_help_handler_ expect]
      dismissContextualPanelEntrypointIPHAnimated:NO];
  [[mocked_entrypoint_help_handler_ expect]
      dismissContextualPanelEntrypointIPHAnimated:NO];

  auto web_state = std::make_unique<web::FakeWebState>();
  std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models;
  FakeContextualPanelTabHelper::CreateForWebState(web_state.get(), models);
  InfoBarManagerImpl::CreateForWebState(web_state.get());
  InfobarBadgeTabHelper::GetOrCreateForWebState(web_state.get());

  web_state_list_.InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate(true));

  EXPECT_FALSE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsLarge);

  [mocked_entrypoint_help_handler_ verify];
}
