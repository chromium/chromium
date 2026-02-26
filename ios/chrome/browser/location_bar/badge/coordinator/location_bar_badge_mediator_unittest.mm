// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/feature_engagement/test/scoped_iph_feature_list.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator_delegate.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_consumer.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
NSString* const kTestAccessibilityLabel = @"testBadge";
}

// Test fake to allow easier triggering of ContextualPanelTabHelperObserver
// methods.
class FakeContextualPanelTabHelper : public ContextualPanelTabHelper {
 public:
  explicit FakeContextualPanelTabHelper(
      web::WebState* web_state,
      std::map<ContextualPanelItemType,
               raw_ptr<ContextualPanelModel, DanglingUntriaged>> models)
      : ContextualPanelTabHelper(web_state, models) {}

  static void CreateForWebState(
      web::WebState* web_state,
      std::map<ContextualPanelItemType,
               raw_ptr<ContextualPanelModel, DanglingUntriaged>> models) {
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

namespace {
std::unique_ptr<KeyedService> CreateTestTracker(ProfileIOS* context) {
  return std::make_unique<
      testing::NiceMock<feature_engagement::test::MockTracker>>();
}
}  // namespace

@interface LocationBarBadgeMediator (Testing)
- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext;
@end

// Test fixture for LocationBarBadgeMediator.
class LocationBarBadgeMediatorTest : public PlatformTest {
 protected:
  LocationBarBadgeMediatorTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kPageActionMenu, {}},
         {kAskGeminiChip, {{kAskGeminiChipPrepopulateFloaty, "true"}}},
         {kLocationBarBadgeMigration, {}}},
        {});

    iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHiOSContextualPanelSampleModelFeature});

    // Set up test factories.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(feature_engagement::TrackerFactory::GetInstance(),
                              base::BindRepeating(&CreateTestTracker));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(BwgServiceFactory::GetInstance(),
                              BwgServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();
    SetUpPrefs(profile_.get()->GetPrefs());
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));
    web_state_list_ = browser_->GetWebStateList();

    // Create WebState to pass Gemini eligibility.
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetCurrentURL(GURL("https://www.google.com"));
    web_state->SetContentsMimeType("text/html");

    // Contextual Panel setup.
    std::map<ContextualPanelItemType,
             raw_ptr<ContextualPanelModel, DanglingUntriaged>>
        models;
    FakeContextualPanelTabHelper::CreateForWebState(web_state.get(), models);
    InfoBarManagerImpl::CreateForWebState(web_state.get());
    InfobarBadgeTabHelper::CreateForWebState(web_state.get());
    BwgTabHelper::CreateForWebState(web_state.get());

    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());

    mediator_ = [[LocationBarBadgeMediator alloc]
        initWithWebStateList:web_state_list_
                     tracker:feature_engagement::TrackerFactory::GetForProfile(
                                 profile_.get())
                 prefService:profile_.get()->GetPrefs()
               geminiService:BwgServiceFactory::GetForProfile(profile_.get())];
    SignInAndSetCapability(true);

    mock_consumer_ = OCMProtocolMock(@protocol(LocationBarBadgeConsumer));
    mediator_.consumer = mock_consumer_;
    mock_delegate_ =
        OCMProtocolMock(@protocol(LocationBarBadgeMediatorDelegate));
    mediator_.delegate = mock_delegate_;
    mock_bwg_command_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    mediator_.BWGCommandHandler = mock_bwg_command_handler_;

    mock_contextual_sheet_handler_ =
        OCMProtocolMock(@protocol(ContextualSheetCommands));
    mock_entrypoint_iph_handler_ =
        OCMProtocolMock(@protocol(ContextualPanelEntrypointIPHCommands));
    mediator_.contextualSheetHandler = mock_contextual_sheet_handler_;
    mediator_.entrypointHelpHandler = mock_entrypoint_iph_handler_;
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_contextual_sheet_handler_
                     forProtocol:@protocol(ContextualSheetCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_entrypoint_iph_handler_
                     forProtocol:@protocol(
                                     ContextualPanelEntrypointIPHCommands)];
  }

  ~LocationBarBadgeMediatorTest() override { [mediator_ disconnect]; }

  void SetUpPrefs(PrefService* pref_service) {
    pref_service->SetInteger(prefs::kGeminiEnabledByPolicy, 0);
    pref_service->SetBoolean(prefs::kAIHubEligibilityTriggered, false);
    pref_service->SetBoolean(prefs::kIOSBwgConsent, true);
  }

  // Signs in a user and sets their model execution capability.
  void SignInAndSetCapability(bool capability) {
    const std::string email = "test@example.com";
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());

    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, email, signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(capability);

    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  // Creates a test badge configuration.
  LocationBarBadgeConfiguration* CreateBadgeConfiguration(
      LocationBarBadgeType badge_type) {
    return [[LocationBarBadgeConfiguration alloc]
         initWithBadgeType:badge_type
        accessibilityLabel:kTestAccessibilityLabel
                badgeImage:[[UIImage alloc] init]];
  }

  // Sets up OCMocks for Gemini contextual cue chip criteria.
  void AllowGeminiChipToShow(bool show_gemini_chip,
                             bool trigger_help_ui,
                             bool badge_is_visible) {
    EXPECT_CALL(*tracker_, WouldTriggerHelpUI(testing::_))
        .WillRepeatedly(testing::Return(trigger_help_ui));
    if (show_gemini_chip) {
      OCMExpect([mock_consumer_ isBadgeVisible]).andReturn(badge_is_visible);
    } else {
      // Triggers an early return, therefore this check shouldn't happen.
      OCMReject([mock_consumer_ isBadgeVisible]);
    }
  }

  void MockValidChipCheck() {
    OCMExpect([mock_consumer_ expandBadgeContainer]);
    OCMStub([mock_delegate_ canShowChip:[OCMArg any]]).andReturn(YES);
    EXPECT_CALL(*tracker_, ShouldTriggerHelpUI(testing::_))
        .WillRepeatedly(testing::Return(true));
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<feature_engagement::test::MockTracker> tracker_;
  LocationBarBadgeMediator* mediator_;
  id mock_bwg_command_handler_;
  id mock_consumer_;
  id mock_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  feature_engagement::test::ScopedIphFeatureList iph_feature_list_;
  id mock_contextual_sheet_handler_;
  id mock_entrypoint_iph_handler_;
};

// Tests that the consumer is updated when the badge configuration is updated.
TEST_F(LocationBarBadgeMediatorTest, TestBadgeUpdateRequest) {
  OCMExpect([mock_consumer_ showBadge]);

  LocationBarBadgeConfiguration* config =
      CreateBadgeConfiguration(LocationBarBadgeType::kReaderMode);
  [mediator_ updateBadgeConfig:config];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that badge isn't shown if a badge is already visible.
TEST_F(LocationBarBadgeMediatorTest, TestCurrentVisibleBadge) {
  OCMExpect([mock_consumer_ isBadgeVisible]).andReturn(true);
  OCMReject([mock_consumer_ setBadgeConfig:[OCMArg any]]);

  LocationBarBadgeConfiguration* config =
      CreateBadgeConfiguration(LocationBarBadgeType::kReaderMode);
  [mediator_ updateBadgeConfig:config];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that the consumer is updated when the color is updated for IPH.
TEST_F(LocationBarBadgeMediatorTest, UpdateColorForIPH) {
  OCMExpect([mock_consumer_ highlightBadge:YES]);
  [mediator_ updateColorForIPH];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that the consumer is updated when the IPH is dismissed.
TEST_F(LocationBarBadgeMediatorTest, DismissIPHAnimated) {
  OCMExpect([mock_consumer_ highlightBadge:NO]);
  [mediator_ dismissIPHAnimated:YES];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that the delegate is notified when the label is centered.
TEST_F(LocationBarBadgeMediatorTest,
       SetLocationBarLabelCenteredBetweenContent) {
  OCMExpect([mock_delegate_ setLocationBarLabelCenteredBetweenContent:mediator_
                                                             centered:YES]);
  [mediator_ setLocationBarLabelCenteredBetweenContent:YES];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that the timestamp pref is updated when the Gemini contextual cue chip
// is shown.
TEST_F(LocationBarBadgeMediatorTest, TestGeminiContextualChipTimestampUpdated) {
  AllowGeminiChipToShow(/*show_gemini_chip=*/true, /*trigger_help_ui=*/true,
                        /*badge_is_visible=*/false);
  LocationBarBadgeConfiguration* config =
      CreateBadgeConfiguration(LocationBarBadgeType::kGeminiContextualCueChip);
  config.badgeText = kTestAccessibilityLabel;
  MockValidChipCheck();

  [mediator_ updateBadgeConfig:config];
  task_environment_.FastForwardBy(base::Seconds(3));

  EXPECT_NE(base::Time(),
            profile_->GetPrefs()->GetTime(
                prefs::kLastGeminiContextualChipDisplayedTimestamp));
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that tapping the gemini chip calls the BWG command handler and logs
// FET metrics.
TEST_F(LocationBarBadgeMediatorTest, TestGeminiChipTapped) {
  id mock_bwg_command_handler = OCMProtocolMock(@protocol(BWGCommands));
  mediator_.BWGCommandHandler = mock_bwg_command_handler;

  OCMExpect([mock_bwg_command_handler
      startGeminiFlowWithStartupState:[OCMArg checkWithBlock:^BOOL(
                                                  GeminiStartupState* state) {
        return state.entryPoint == gemini::EntryPoint::OmniboxChip &&
               [state.prepopulatedPrompt
                   isEqualToString:l10n_util::GetNSString(
                                       IDS_IOS_ASK_GEMINI_CHIP_PREFILL_PROMPT)];
      }]]);

  EXPECT_CALL(
      *tracker_,
      NotifyEvent(feature_engagement::events::kIOSGeminiContextualCueChipUsed));
  LocationBarBadgeConfiguration* config =
      CreateBadgeConfiguration(LocationBarBadgeType::kGeminiContextualCueChip);
  config.badgeText = kTestAccessibilityLabel;
  [mediator_ badgeTapped:config];
  EXPECT_OCMOCK_VERIFY(mock_bwg_command_handler);
}
// Tests that the Gemini contextual cue chip is not shown if it was recently
// displayed.
TEST_F(LocationBarBadgeMediatorTest, TestGeminiChipNotShownIfTooRecent) {
  // Expect the badge to be shown and metrics logged the first time.
  AllowGeminiChipToShow(/*show_gemini_chip=*/true, /*trigger_help_ui=*/true,
                        /*badge_is_visible=*/false);
  LocationBarBadgeConfiguration* config =
      CreateBadgeConfiguration(LocationBarBadgeType::kGeminiContextualCueChip);
  config.badgeText = kTestAccessibilityLabel;
  MockValidChipCheck();

  [mediator_ updateBadgeConfig:config];
  task_environment_.FastForwardBy(base::Seconds(3));

  EXPECT_NE(base::Time(),
            profile_->GetPrefs()->GetTime(
                prefs::kLastGeminiContextualChipDisplayedTimestamp));
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  // Simulate that the timestamp check fails, causing badge to not show.
  AllowGeminiChipToShow(/*show_gemini_chip=*/false, /*trigger_help_ui=*/true,
                        /*badge_is_visible=*/false);
  [mediator_ updateBadgeConfig:config];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

#pragma mark - Contextual Panel Tests

// Tests that tapping the entrypoint opens the panel if it's closed and vice
// versa.
TEST_F(LocationBarBadgeMediatorTest, TestContextualPanelEntrypointTapped) {
  const base::HistogramTester histogram_tester;
  ContextualPanelTabHelper* tab_helper = ContextualPanelTabHelper::FromWebState(
      web_state_list_->GetActiveWebState());

  // Set the metrics data for the current entrypoint appearing.
  ContextualPanelTabHelper::EntrypointMetricsData metrics_data;
  metrics_data.entrypoint_item_type = ContextualPanelItemType::SamplePanelItem;
  metrics_data.appearance_time = base::Time::Now() - base::Seconds(10);
  tab_helper->SetMetricsData(metrics_data);

  OCMExpect([mock_contextual_sheet_handler_ openContextualSheet]);
  OCMExpect(
      [mock_entrypoint_iph_handler_ dismissContextualPanelEntrypointIPH:YES]);
  LocationBarBadgeConfiguration* config = CreateBadgeConfiguration(
      LocationBarBadgeType::kContextualPanelEntryPointSample);
  [mediator_ badgeTapped:config];
  tab_helper->OpenContextualPanel();

  OCMExpect([mock_contextual_sheet_handler_ closeContextualSheet]);
  OCMExpect(
      [mock_entrypoint_iph_handler_ dismissContextualPanelEntrypointIPH:YES]);
  [mediator_ badgeTapped:config];
  tab_helper->CloseContextualPanel();

  EXPECT_OCMOCK_VERIFY(mock_contextual_sheet_handler_);
  EXPECT_OCMOCK_VERIFY(mock_entrypoint_iph_handler_);

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

TEST_F(LocationBarBadgeMediatorTest, TestContextualPanelTabHelperDestroyed) {
  OCMExpect(
      [mock_entrypoint_iph_handler_ dismissContextualPanelEntrypointIPH:NO]);
  OCMExpect([mock_consumer_ hideBadge]);

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_->GetActiveWebState()));
  tab_helper->CallContextualPanelTabHelperDestroyed();

  EXPECT_OCMOCK_VERIFY(mock_entrypoint_iph_handler_);
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that if one configuration is provided, the entrypoint becomes shown.
TEST_F(LocationBarBadgeMediatorTest, TestContextualPanelOneConfiguration) {
  const base::HistogramTester histogram_tester;
  OCMExpect(
      [mock_entrypoint_iph_handler_ dismissContextualPanelEntrypointIPH:NO]);
  OCMExpect([mock_consumer_ showBadge]);
  OCMExpect([mock_consumer_ setBadgeConfig:[OCMArg any]]);
  OCMReject([mock_consumer_ expandBadgeContainer]);

  ContextualPanelItemConfiguration configuration(
      ContextualPanelItemType::SamplePanelItem);
  configuration.entrypoint_image_name = "chrome_product";
  configuration.image_type =
      ContextualPanelItemConfiguration::EntrypointImageType::Image;

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_->GetActiveWebState()));

  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
      item_configurations;
  item_configurations.push_back(configuration.weak_ptr_factory.GetWeakPtr());

  tab_helper->CallContextualPanelHasNewData(item_configurations);

  EXPECT_OCMOCK_VERIFY(mock_entrypoint_iph_handler_);
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

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
// helper after disconnecting.
TEST_F(LocationBarBadgeMediatorTest, TestContextualPanelDisconnect) {
  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_->GetActiveWebState()));

  EXPECT_FALSE(tab_helper->observers_.empty());
  [mediator_ disconnect];
  mediator_ = nil;
  EXPECT_TRUE(tab_helper->observers_.empty());
}

TEST_F(LocationBarBadgeMediatorTest,
       TestContextualPanelLargeEntrypointAppears) {
  const base::HistogramTester histogram_tester;
  OCMExpect(
      [mock_entrypoint_iph_handler_ dismissContextualPanelEntrypointIPH:NO]);
  OCMStub([mock_delegate_ canShowChip:[OCMArg any]]).andReturn(YES);

  OCMExpect([mock_consumer_ showBadge]);
  OCMExpect([mock_consumer_ setBadgeConfig:[OCMArg any]]);

  std::unique_ptr<SamplePanelItemConfiguration> configuration =
      std::make_unique<SamplePanelItemConfiguration>();
  configuration->relevance = ContextualPanelItemConfiguration::high_relevance;
  configuration->entrypoint_message = "test";
  configuration->entrypoint_image_name = "chrome_product";
  configuration->image_type =
      ContextualPanelItemConfiguration::EntrypointImageType::Image;

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_->GetActiveWebState()));

  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
      item_configurations;
  item_configurations.push_back(configuration->weak_ptr_factory.GetWeakPtr());
  tab_helper->AddToCachedConfigs(std::move(configuration));

  tab_helper->CallContextualPanelHasNewData(item_configurations);

  // At first, the small entrypoint should be displayed.
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);

  // Advance time so that the large entrypoint is displayed.
  OCMExpect([mock_consumer_ expandBadgeContainer]);
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  OCMExpect([mock_consumer_ collapseBadgeContainer]);
  // Advance time until the large entrypoint transitions back to small.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
  EXPECT_OCMOCK_VERIFY(mock_entrypoint_iph_handler_);

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

TEST_F(LocationBarBadgeMediatorTest, TestContextualPanelIPHEntrypointAppears) {
  OCMStub([mock_delegate_ canShowChip:[OCMArg any]]).andReturn(YES);
  EXPECT_CALL(
      *tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSContextualPanelSampleModelFeature)))
      .WillRepeatedly(testing::Return(true));

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
  configuration->entrypoint_image_name = "chrome_product";
  configuration->image_type =
      ContextualPanelItemConfiguration::EntrypointImageType::Image;

  OCMStub([mock_entrypoint_iph_handler_
              showContextualPanelEntrypointIPHWithConfig:configuration.get()
                                             anchorPoint:CGPointMake(0, 0)
                                         isBottomOmnibox:NO])
      .andReturn(YES);

  OCMExpect(
      [mock_entrypoint_iph_handler_ dismissContextualPanelEntrypointIPH:NO]);

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_->GetActiveWebState()));

  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
      item_configurations;
  item_configurations.push_back(configuration->weak_ptr_factory.GetWeakPtr());
  tab_helper->AddToCachedConfigs(std::move(configuration));

  tab_helper->CallContextualPanelHasNewData(item_configurations);

  OCMExpect([mock_consumer_ highlightBadge:YES]);
  // Advance time so that the IPH entrypoint is displayed.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  // Advance time until the IPH is dismissed.
  OCMExpect(
      [mock_entrypoint_iph_handler_ dismissContextualPanelEntrypointIPH:YES]);
  OCMExpect([mock_delegate_ enableFullscreen]);
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_OCMOCK_VERIFY(mock_entrypoint_iph_handler_);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);

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
TEST_F(LocationBarBadgeMediatorTest, TestContextualPanelWebStateListChanged) {
  OCMExpect(
      [mock_entrypoint_iph_handler_ dismissContextualPanelEntrypointIPH:NO]);
  OCMExpect([mock_consumer_ hideBadge]);

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  std::map<ContextualPanelItemType,
           raw_ptr<ContextualPanelModel, DanglingUntriaged>>
      models;
  FakeContextualPanelTabHelper::CreateForWebState(web_state.get(), models);
  InfoBarManagerImpl::CreateForWebState(web_state.get());
  InfobarBadgeTabHelper::CreateForWebState(web_state.get());

  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate(true));

  EXPECT_OCMOCK_VERIFY(mock_entrypoint_iph_handler_);
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that the consumer is not updated when there is a same-page navigation.
TEST_F(LocationBarBadgeMediatorTest, TestDidStartNavigationSamePage) {
  OCMReject([mock_consumer_ hideBadge]);

  web::WebState* activeWebState = web_state_list_->GetActiveWebState();
  auto navigation_context = std::make_unique<web::FakeNavigationContext>();
  navigation_context->SetIsSameDocument(true);
  [mediator_ webState:activeWebState
      didStartNavigation:navigation_context.get()];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that the consumer is updated when there is a different-page navigation.
TEST_F(LocationBarBadgeMediatorTest, TestDidStartNavigationDifferentPage) {
  OCMExpect([mock_consumer_ hideBadge]);

  web::WebState* activeWebState = web_state_list_->GetActiveWebState();
  auto navigation_context = std::make_unique<web::FakeNavigationContext>();
  navigation_context->SetIsSameDocument(false);
  [mediator_ webState:activeWebState
      didStartNavigation:navigation_context.get()];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}
