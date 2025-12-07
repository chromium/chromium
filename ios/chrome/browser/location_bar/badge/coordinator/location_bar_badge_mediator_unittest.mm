// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
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
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
NSString* const kTestAccessibilityLabel = @"testBadge";
}

namespace {
std::unique_ptr<KeyedService> CreateTestTracker(ProfileIOS* context) {
  return std::make_unique<
      testing::NiceMock<feature_engagement::test::MockTracker>>();
}
}  // namespace

// Test fixture for LocationBarBadgeMediator.
class LocationBarBadgeMediatorTest : public PlatformTest {
 protected:
  LocationBarBadgeMediatorTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {/*enabled_features=*/{kPageActionMenu, {}},
         {kAskGeminiChip, {{kAskGeminiChipPrepopulateFloaty, "true"}}}},
        /*disabled_features=*/{});

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
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));
    web_state_list_ = browser_->GetWebStateList();

    // Create WebState to pass Gemini eligibility.
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    BwgTabHelper::CreateForWebState(web_state.get());
    web_state->SetBrowserState(profile_.get());
    web_state->SetCurrentURL(GURL("https://www.google.com"));
    web_state->SetContentsMimeType("text/html");
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
    EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(testing::_))
        .WillRepeatedly(testing::Return(trigger_help_ui));
    if (show_gemini_chip) {
      OCMExpect([mock_consumer_ isBadgeVisible]).andReturn(badge_is_visible);
    } else {
      // Triggers an early return, therefore this check shouldn't happen.
      OCMReject([mock_consumer_ isBadgeVisible]);
    }
  }

  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;
  LocationBarBadgeMediator* mediator_;
  id mock_bwg_command_handler_;
  id mock_consumer_;
  id mock_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
  [mediator_ updateBadgeConfig:config];
  EXPECT_NE(base::Time(),
            profile_->GetPrefs()->GetTime(
                prefs::kLastGeminiContextualChipDisplayedTimestamp));
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}
// Tests that the FET metrics are logged when the Gemini contextual cue chip is
// shown.
TEST_F(LocationBarBadgeMediatorTest, TestGeminiContextualChipFETMetricsLogged) {
  AllowGeminiChipToShow(/*show_gemini_chip=*/true, /*trigger_help_ui=*/true,
                        /*badge_is_visible=*/false);
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(
          feature_engagement::events::kIOSGeminiContextualCueChipTriggered));
  LocationBarBadgeConfiguration* config =
      CreateBadgeConfiguration(LocationBarBadgeType::kGeminiContextualCueChip);
  [mediator_ updateBadgeConfig:config];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}
// Tests that tapping the gemini chip calls the BWG command handler and logs
// FET metrics.
TEST_F(LocationBarBadgeMediatorTest, TestGeminiChipTapped) {
  id mock_bwg_command_handler = OCMProtocolMock(@protocol(BWGCommands));
  mediator_.BWGCommandHandler = mock_bwg_command_handler;
  OCMExpect([mock_bwg_command_handler
      startBWGFlowWithEntryPoint:bwg::EntryPoint::OmniboxChip]);
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(feature_engagement::events::kIOSGeminiContextualCueChipUsed));
  LocationBarBadgeConfiguration* config =
      CreateBadgeConfiguration(LocationBarBadgeType::kGeminiContextualCueChip);
  config.badgeText = kTestAccessibilityLabel;
  [mediator_ badgeTapped:config];
  EXPECT_OCMOCK_VERIFY(mock_bwg_command_handler);

  web::WebState* activeWebState = web_state_list_->GetActiveWebState();
  BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(activeWebState);
  ASSERT_TRUE(BWGTabHelper->GetContextualCueLabel().length != 0);
}
// Tests that the Gemini contextual cue chip is not shown if it was recently
// displayed.
TEST_F(LocationBarBadgeMediatorTest, TestGeminiChipNotShownIfTooRecent) {
  // Expect the badge to be shown and metrics logged the first time.
  AllowGeminiChipToShow(/*show_gemini_chip=*/true, /*trigger_help_ui=*/true,
                        /*badge_is_visible=*/false);
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(
          feature_engagement::events::kIOSGeminiContextualCueChipTriggered))
      .Times(1);
  LocationBarBadgeConfiguration* config =
      CreateBadgeConfiguration(LocationBarBadgeType::kGeminiContextualCueChip);
  [mediator_ updateBadgeConfig:config];
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
