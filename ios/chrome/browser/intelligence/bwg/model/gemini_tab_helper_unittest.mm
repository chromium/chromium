// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/functional/callback_helpers.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/simple_test_clock.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/feature_engagement/test/scoped_iph_feature_list.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

class GeminiTabHelperTest : public PlatformTest {
 protected:
  GeminiTabHelperTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        feature_engagement::TrackerFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(GeminiServiceFactory::GetInstance(),
                              GeminiServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    // Set up a signed in user with the capability to enable Gemini.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    AccountInfo account = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@example.com", signin::ConsentLevel::kSignin);
    // Grant the user the capability to use Gemini.
    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_manager, account);
    profile_->GetPrefs()->SetInteger(prefs::kGeminiEnabledByPolicy, 0);

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    web_state_->WasShown();
    GeminiTabHelper::CreateForWebState(web_state_.get());
    tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());

    mock_bwg_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    tab_helper_->SetGeminiCommandsHandler(mock_bwg_handler_);
    mock_location_bar_badge_handler_ =
        OCMProtocolMock(@protocol(LocationBarBadgeCommands));
    tab_helper_->SetLocationBarBadgeCommandsHandler(
        mock_location_bar_badge_handler_);
    mock_help_handler_ = OCMProtocolMock(@protocol(HelpCommands));
    tab_helper_->SetHelpCommandsHandler(mock_help_handler_);
  }

  // Environment objects are declared first, so they are destroyed last.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;

  base::SimpleTestClock test_clock_;
  base::RunLoop run_loop_;

  // Profile and services that depend on the environment are declared next.
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<GeminiTabHelper, DanglingUntriaged> tab_helper_;

  // Mock BWG handler.
  id mock_bwg_handler_;
  // Mock Location Bar Badge handler.
  id mock_location_bar_badge_handler_;
  // Mock Help commands handler.
  id mock_help_handler_;

  base::RepeatingCallback<void(bool)> BoolArgumentQuitClosure() {
    return base::IgnoreArgs<bool>(run_loop_.QuitClosure());
  }

  feature_engagement::Tracker* InitializeTracker() {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(profile_.get());
    // Make sure tracker is initialized.
    tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
    run_loop_.Run();
    return tracker;
  }

  void AddOptimizationGuideHint(const GURL& url) {
    OptimizationGuideService* optimization_guide_service =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());
    optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
    cueing_metadata.add_cueing_configurations();
    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/"
        "optimization_guide.proto.GlicContextualCueingMetadata");
    cueing_metadata.SerializeToString(any_metadata.mutable_value());
    optimization_guide::OptimizationMetadata metadata;
    metadata.set_any_metadata(any_metadata);
    optimization_guide_service->AddHintForTesting(
        GURL(url), optimization_guide::proto::GLIC_CONTEXTUAL_CUEING, metadata);
  }

  void AddZeroStateSuggestionsHint(const GURL& url,
                                   bool should_simulate_eligibility) {
    OptimizationGuideService* optimization_guide_service =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());
    optimization_guide::proto::GlicZeroStateSuggestionsMetadata
        suggestions_metadata;
    suggestions_metadata.set_contextual_suggestions_eligible(true);
    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/"
        "optimization_guide.proto.GlicZeroStateSuggestionsMetadata");
    suggestions_metadata.SerializeToString(any_metadata.mutable_value());
    optimization_guide::OptimizationMetadata metadata;
    metadata.set_any_metadata(any_metadata);
    optimization_guide_service->AddHintForTesting(
        GURL(url), optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
        metadata);
    if (should_simulate_eligibility) {
      SimulateGeminiEligibilityDecisionReceived(url, metadata);
    }
  }

  void SimulateFirstRunRecency(feature_engagement::Tracker* tracker, int days) {
    // Make first run not recent.
    tracker->NotifyEvent(feature_engagement::events::kIOSFirstRunComplete);
    task_environment_.FastForwardBy(base::Days(days));
    ForceFirstRunRecency(days);
  }

  void SimulateGeminiEligibilityDecisionReceived(
      const GURL& url,
      const optimization_guide::OptimizationMetadata& metadata) {
    tab_helper_->zero_state_suggestions_service_->SetCanApply(true);
    tab_helper_->current_url_ = url;
    bool user_enabled = profile_->GetPrefs()->GetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
    tab_helper_->OnGeminiEligibilityDecision(
        url, user_enabled, optimization_guide::OptimizationGuideDecision::kTrue,
        metadata);
  }
};

TEST_F(GeminiTabHelperTest, TestContextualChipCommandSent) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu, kAskGeminiChip},
      /*disabled_features=*/{});
  GURL url("https://www.chromium.org");
  AddOptimizationGuideHint(url);

  // Check if LocationBarBadge command was sent as a response to receiving a
  // contextual cue.
  OCMExpect([mock_location_bar_badge_handler_ updateBadgeConfig:[OCMArg any]]);
  std::unique_ptr<web::FakeNavigationContext> context =
      std::make_unique<web::FakeNavigationContext>();
  context->SetHasCommitted(true);
  context->SetUrl(url);
  tab_helper_->DidFinishNavigation(web_state_.get(), context.get());
  EXPECT_OCMOCK_VERIFY(mock_location_bar_badge_handler_);
}

TEST_F(GeminiTabHelperTest, TestContextualChipCommandNotSentWhenHidden) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu, kAskGeminiChip},
      /*disabled_features=*/{});
  GURL url("https://www.chromium.org");
  AddOptimizationGuideHint(url);

  web_state_->WasHidden();

  OCMReject([mock_location_bar_badge_handler_ updateBadgeConfig:[OCMArg any]]);

  std::unique_ptr<web::FakeNavigationContext> context =
      std::make_unique<web::FakeNavigationContext>();
  context->SetHasCommitted(true);
  context->SetUrl(url);
  tab_helper_->DidFinishNavigation(web_state_.get(), context.get());
  EXPECT_OCMOCK_VERIFY(mock_location_bar_badge_handler_);
}

TEST_F(GeminiTabHelperTest, TestIsLastInteractionUrlDifferent_SameURL) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu}, /*disabled_features=*/{});
  GURL url("https://www.chromium.org");
  web_state_->SetCurrentURL(url);
  tab_helper_->CreateOrUpdateGeminiSessionInStorage("server_id");
  ASSERT_FALSE(tab_helper_->IsLastInteractionUrlDifferent());
}

TEST_F(GeminiTabHelperTest, TestIsLastInteractionUrlDifferent_DifferentURL) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu}, /*disabled_features=*/{});
  GURL url1("https://www.chromium.org");
  web_state_->SetCurrentURL(url1);
  tab_helper_->CreateOrUpdateGeminiSessionInStorage("server_id");

  GURL url2("https://www.google.com");
  web_state_->SetCurrentURL(url2);
  ASSERT_TRUE(tab_helper_->IsLastInteractionUrlDifferent());
}

TEST_F(GeminiTabHelperTest,
       TestIsLastInteractionUrlDifferent_GeminiCrossTabEnabled_SameURL) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu},
      /*disabled_features=*/{});
  GURL url("https://www.chromium.org");
  web_state_->SetCurrentURL(url);
  tab_helper_->CreateOrUpdateGeminiSessionInStorage("server_id");
  ASSERT_FALSE(tab_helper_->IsLastInteractionUrlDifferent());
}

TEST_F(GeminiTabHelperTest,
       TestIsLastInteractionUrlDifferent_GeminiCrossTabEnabled_DifferentURL) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu},
      /*disabled_features=*/{});
  GURL url1("https://www.chromium.org");
  web_state_->SetCurrentURL(url1);
  tab_helper_->CreateOrUpdateGeminiSessionInStorage("server_id");

  GURL url2("https://www.google.com");
  web_state_->SetCurrentURL(url2);
  ASSERT_TRUE(tab_helper_->IsLastInteractionUrlDifferent());
}

// TODO(crbug.com/430313339): Add a test for the last interaction case.
TEST_F(GeminiTabHelperTest, TestShouldShowSuggestionChips) {
  web_state_->SetCurrentURL(GURL("https://www.google.com/search?q=test"));
  ASSERT_FALSE(tab_helper_->ShouldShowSuggestionChips());

  web_state_->SetCurrentURL(GURL("https://www.not-google.com"));
  ASSERT_TRUE(tab_helper_->ShouldShowSuggestionChips());
}

TEST_F(GeminiTabHelperTest, TestCreateOrUpdateGeminiSessionInStorage) {
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateGeminiSessionInStorage(server_id);
  std::optional<std::string> retrieved_server_id = tab_helper_->GetServerId();
  ASSERT_TRUE(retrieved_server_id.has_value());
  ASSERT_EQ(server_id, retrieved_server_id.value());
}

TEST_F(GeminiTabHelperTest, TestDeleteGeminiSessionInStorage) {
  tab_helper_->CreateOrUpdateGeminiSessionInStorage("test_server_id");
  ASSERT_TRUE(tab_helper_->GetServerId().has_value());
  tab_helper_->DeleteGeminiSessionInStorage();
  ASSERT_FALSE(tab_helper_->GetServerId().has_value());
}

TEST_F(GeminiTabHelperTest, TestGetServerId) {
  ASSERT_FALSE(tab_helper_->GetServerId().has_value());
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateGeminiSessionInStorage(server_id);
  ASSERT_TRUE(tab_helper_->GetServerId().has_value());
  ASSERT_EQ(server_id, tab_helper_->GetServerId().value());
}

TEST_F(GeminiTabHelperTest, TestGetServerId_Expired) {
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateGeminiSessionInStorage(server_id);
  ASSERT_TRUE(tab_helper_->GetServerId().has_value());

  // Fast forward time to expire the session.
  task_environment_.FastForwardBy(BWGSessionValidityDuration() +
                                  base::Seconds(1));

  ASSERT_FALSE(tab_helper_->GetServerId().has_value());
}

TEST_F(GeminiTabHelperTest, TestDidStartNavigation_ShowsImageRemixIPH) {
  feature_engagement::test::ScopedIphFeatureList iph_feature_list;
  iph_feature_list.InitAndEnableFeatures(
      {feature_engagement::kIPHiOSGeminiImageRemixFeature, kPageActionMenu,
       kGeminiImageRemixTool, kZeroStateSuggestions});

  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  tab_helper_->SetGeminiCommandsHandler(mock_bwg_handler_);
  tab_helper_->SetLocationBarBadgeCommandsHandler(
      mock_location_bar_badge_handler_);
  tab_helper_->SetHelpCommandsHandler(mock_help_handler_);
  web_state_->SetCurrentURL(GURL("https://www.chromium.org"));
  web_state_->SetContentsMimeType("text/html");

  feature_engagement::Tracker* tracker = InitializeTracker();
  SimulateFirstRunRecency(tracker, 2);

  profile_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  OCMExpect([mock_help_handler_
      presentInProductHelpWithType:InProductHelpType::kGeminiImageRemix]);

  AddZeroStateSuggestionsHint(web_state_->GetVisibleURL(), true);

  EXPECT_OCMOCK_VERIFY(mock_help_handler_);
}

TEST_F(GeminiTabHelperTest,
       TestDidStartNavigation_DoesNotShowImageRemixIPH_WhenNotMSBB) {
  feature_engagement::test::ScopedIphFeatureList iph_feature_list;
  iph_feature_list.InitAndEnableFeatures(
      {feature_engagement::kIPHiOSGeminiImageRemixFeature, kPageActionMenu,
       kGeminiImageRemixTool, kZeroStateSuggestions});

  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  tab_helper_->SetGeminiCommandsHandler(mock_bwg_handler_);
  tab_helper_->SetLocationBarBadgeCommandsHandler(
      mock_location_bar_badge_handler_);
  tab_helper_->SetHelpCommandsHandler(mock_help_handler_);
  web_state_->SetCurrentURL(GURL("https://www.chromium.org"));

  feature_engagement::Tracker* tracker = InitializeTracker();
  SimulateFirstRunRecency(tracker, 2);

  profile_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  OCMReject([mock_help_handler_
      presentInProductHelpWithType:InProductHelpType::kGeminiImageRemix]);

  AddZeroStateSuggestionsHint(web_state_->GetVisibleURL(), true);

  EXPECT_OCMOCK_VERIFY(mock_help_handler_);
}

TEST_F(GeminiTabHelperTest, TestDidStartNavigation_ShowsPromo) {
  feature_engagement::test::ScopedIphFeatureList iph_feature_list;
  iph_feature_list.InitAndEnableFeatures(
      {feature_engagement::kIPHiOSGeminiFullscreenPromoFeature, kPageActionMenu,
       kGeminiNavigationPromo, kAskGeminiChip});

  feature_engagement::Tracker* tracker = InitializeTracker();

  OCMExpect([mock_bwg_handler_ showBWGPromoIfPageIsEligible]);

  SimulateFirstRunRecency(tracker, 2);

  GURL url("https://www.chromium.org");
  AddOptimizationGuideHint(url);

  auto navigation_context = std::make_unique<web::FakeNavigationContext>();
  navigation_context->SetUrl(url);
  navigation_context->SetHasCommitted(true);
  tab_helper_->DidFinishNavigation(web_state_.get(), navigation_context.get());
  EXPECT_OCMOCK_VERIFY(mock_bwg_handler_);
}

TEST_F(GeminiTabHelperTest,
       TestDidStartNavigation_DoesNotShowPromoIfConsentGiven) {
  feature_list_.InitWithFeatures(
      {kGeminiNavigationPromo, kAskGeminiChip, kPageActionMenu}, {});

  feature_engagement::Tracker* tracker = InitializeTracker();

  OCMReject([mock_bwg_handler_ showBWGPromoIfPageIsEligible]);

  SimulateFirstRunRecency(tracker, 2);

  // Send signal that the user has already given his consent to the feature.
  tracker->NotifyEvent(feature_engagement::events::kIOSGeminiConsentGiven);

  GURL url("https://www.chromium.org");
  AddOptimizationGuideHint(url);

  auto navigation_context = std::make_unique<web::FakeNavigationContext>();
  navigation_context->SetUrl(url);
  navigation_context->SetHasCommitted(true);
  tab_helper_->DidFinishNavigation(web_state_.get(), navigation_context.get());
  EXPECT_OCMOCK_VERIFY(mock_bwg_handler_);
}

TEST_F(GeminiTabHelperTest, TestDidStartNavigation_DoesNotShowPromoForNewUser) {
  feature_list_.InitWithFeatures(
      {kGeminiNavigationPromo, kAskGeminiChip, kPageActionMenu}, {});

  feature_engagement::Tracker* tracker = InitializeTracker();

  OCMReject([mock_bwg_handler_ showBWGPromoIfPageIsEligible]);

  SimulateFirstRunRecency(tracker, 0);

  GURL url("https://www.chromium.org");
  AddOptimizationGuideHint(url);

  auto navigation_context = std::make_unique<web::FakeNavigationContext>();
  navigation_context->SetUrl(url);
  navigation_context->SetHasCommitted(true);
  tab_helper_->DidFinishNavigation(web_state_.get(), navigation_context.get());
  EXPECT_OCMOCK_VERIFY(mock_bwg_handler_);
}

TEST_F(GeminiTabHelperTest,
       TestDidStartNavigation_DoesNotShowPromoIfBWGStarted) {
  feature_list_.InitWithFeatures(
      {kGeminiNavigationPromo, kAskGeminiChip, kPageActionMenu}, {});

  feature_engagement::Tracker* tracker = InitializeTracker();

  OCMReject([mock_bwg_handler_ showBWGPromoIfPageIsEligible]);

  SimulateFirstRunRecency(tracker, 2);

  // Send signal that the user has started Gemini flow from a non-promo entry
  // point.
  tracker->NotifyEvent(
      feature_engagement::events::kIOSGeminiFlowStartedNonPromo);

  GURL url("https://www.chromium.org");
  AddOptimizationGuideHint(url);

  auto navigation_context = std::make_unique<web::FakeNavigationContext>();
  navigation_context->SetUrl(url);
  navigation_context->SetHasCommitted(true);
  tab_helper_->DidFinishNavigation(web_state_.get(), navigation_context.get());
  EXPECT_OCMOCK_VERIFY(mock_bwg_handler_);
}

TEST_F(GeminiTabHelperTest, TestDidStartNavigation_ShowsPromoPrefs) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu, kGeminiNavigationPromo,
                            kAskGeminiChip,
                            feature_engagement::
                                kIPHiOSGeminiFullscreenPromoFeature},
      /*disabled_features=*/{});

  OCMExpect([mock_bwg_handler_ showBWGPromoIfPageIsEligible]);

  feature_engagement::Tracker* tracker = InitializeTracker();

  // Set prefs to a state where the promo should be shown.
  profile_->GetPrefs()->SetBoolean(prefs::kIOSBwgConsent, false);
  profile_->GetPrefs()->SetInteger(prefs::kIOSBWGPromoImpressionCount, 0);

  // Make first run not recent.
  SimulateFirstRunRecency(tracker, 2);

  GURL url("https://www.chromium.org");
  AddOptimizationGuideHint(url);

  auto navigation_context = std::make_unique<web::FakeNavigationContext>();
  navigation_context->SetUrl(url);
  navigation_context->SetHasCommitted(true);
  tab_helper_->DidFinishNavigation(web_state_.get(), navigation_context.get());
  EXPECT_OCMOCK_VERIFY(mock_bwg_handler_);
}

TEST_F(GeminiTabHelperTest, TestDidStartNavigation_DoesNotShowPromoPrefs) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu, kGeminiNavigationPromo,
                            kAskGeminiChip},
      /*disabled_features=*/{});

  OCMReject([mock_bwg_handler_ showBWGPromoIfPageIsEligible]);

  feature_engagement::Tracker* tracker = InitializeTracker();

  // Set prefs to a state where the promo should not be shown.
  profile_->GetPrefs()->SetBoolean(prefs::kIOSBwgConsent, true);
  profile_->GetPrefs()->SetInteger(prefs::kIOSBWGPromoImpressionCount, 0);

  // Make first run not recent.
  SimulateFirstRunRecency(tracker, 2);

  GURL url("https://www.chromium.org");
  AddOptimizationGuideHint(url);

  auto navigation_context = std::make_unique<web::FakeNavigationContext>();
  navigation_context->SetUrl(url);
  navigation_context->SetHasCommitted(true);
  tab_helper_->DidFinishNavigation(web_state_.get(), navigation_context.get());
  EXPECT_OCMOCK_VERIFY(mock_bwg_handler_);
}

TEST_F(GeminiTabHelperTest, WebStateDestroyed) {
  // Destroy the webstate.
  web_state_.reset();

  // The test passes if it doesn't crash.
}

TEST_F(GeminiTabHelperTest,
       WebStateDestroyed_DoesNotCleanUpSession_GeminiCrossTabEnabled) {
  feature_list_.InitWithFeatures({kPageActionMenu}, {});
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateGeminiSessionInStorage(server_id);
  ASSERT_EQ(tab_helper_->GetServerId().value(), server_id);

  // Destroy the webstate.
  web_state_.reset();

  // Create a new webstate and tab helper to check the prefs.
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());

  ASSERT_EQ(tab_helper_->GetServerId().value(), server_id);
}

@interface FakePageContextWrapper : PageContextWrapper
@property(nonatomic, assign) BOOL populateCalled;
@end

@implementation FakePageContextWrapper
- (instancetype)initWithWebState:(web::WebState*)webState
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback {
  // Call the super designated initializer but we don't care about the callback
  // since this is a fake.
  return [super initWithWebState:webState completionCallback:base::DoNothing()];
}
- (void)populatePageContextFieldsAsync {
  self.populateCalled = YES;
}
@end

TEST_F(GeminiTabHelperTest, TestGeneratePageContext) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->SetContentsMimeType("text/html");

  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  FakePageContextWrapper* fakeWrapper =
      [[FakePageContextWrapper alloc] initWithWebState:web_state_.get()
                                    completionCallback:base::DoNothing()];
  OCMStub([mockWrapperClass alloc]).andReturn(fakeWrapper);

  base::RunLoop run_loop;
  tab_helper_->GeneratePageContext(
      base::BindRepeating([](base::RunLoop* run_loop,
                             GeminiPageContext* response) { run_loop->Quit(); },
                          &run_loop));

  EXPECT_TRUE(fakeWrapper.shouldGetAnnotatedPageContent);
  EXPECT_TRUE(fakeWrapper.shouldGetSnapshot);
  EXPECT_TRUE(fakeWrapper.populateCalled);
}

TEST_F(GeminiTabHelperTest, TestGeneratePageContext_WaitsForLoad) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->SetContentsMimeType("text/html");
  web_state_->SetLoading(true);

  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  FakePageContextWrapper* fakeWrapper =
      [[FakePageContextWrapper alloc] initWithWebState:web_state_.get()
                                    completionCallback:base::DoNothing()];
  OCMStub([mockWrapperClass alloc]).andReturn(fakeWrapper);

  tab_helper_->GeneratePageContext(base::DoNothing());

  // Should NOT be called immediately.
  EXPECT_FALSE(fakeWrapper.populateCalled);

  // Now simulate page load finish.
  tab_helper_->PageLoaded(web_state_.get(),
                          web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_TRUE(fakeWrapper.shouldGetAnnotatedPageContent);
  EXPECT_TRUE(fakeWrapper.shouldGetSnapshot);
  EXPECT_TRUE(fakeWrapper.populateCalled);
}

TEST_F(GeminiTabHelperTest,
       TestDidStartNavigation_DoesNotShowImageRemixIPH_WhenBwgNotAvailable) {
  feature_engagement::test::ScopedIphFeatureList iph_feature_list;
  iph_feature_list.InitAndEnableFeatures(
      {feature_engagement::kIPHiOSGeminiImageRemixFeature, kPageActionMenu,
       kGeminiImageRemixTool, kZeroStateSuggestions});

  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  tab_helper_->SetGeminiCommandsHandler(mock_bwg_handler_);
  tab_helper_->SetLocationBarBadgeCommandsHandler(
      mock_location_bar_badge_handler_);
  tab_helper_->SetHelpCommandsHandler(mock_help_handler_);
  web_state_->SetCurrentURL(GURL("https://www.chromium.org"));
  web_state_->SetContentsMimeType("text/html");

  // Disable Gemini by policy to simulate BWG not being available.
  profile_->GetPrefs()->SetInteger(prefs::kGeminiEnabledByPolicy, 1);

  feature_engagement::Tracker* tracker = InitializeTracker();
  SimulateFirstRunRecency(tracker, 2);

  profile_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  AddZeroStateSuggestionsHint(web_state_->GetVisibleURL(), false);

  OCMReject([mock_help_handler_
      presentInProductHelpWithType:InProductHelpType::kGeminiImageRemix]);

  auto navigation_context = std::make_unique<web::FakeNavigationContext>();
  navigation_context->SetUrl(web_state_->GetVisibleURL());
  tab_helper_->DidStartNavigation(web_state_.get(), navigation_context.get());

  EXPECT_OCMOCK_VERIFY(mock_help_handler_);
}

TEST_F(GeminiTabHelperTest, TestForcePageContextGeneration) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->SetContentsMimeType("text/html");
  web_state_->SetLoading(true);

  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  FakePageContextWrapper* fakeWrapper =
      [[FakePageContextWrapper alloc] initWithWebState:web_state_.get()
                                    completionCallback:base::DoNothing()];
  OCMStub([mockWrapperClass alloc]).andReturn(fakeWrapper);

  tab_helper_->GeneratePageContext(base::DoNothing());

  // Should NOT be called immediately.
  EXPECT_FALSE(fakeWrapper.populateCalled);

  // Force generation.
  tab_helper_->ForcePageContextGeneration();

  EXPECT_TRUE(fakeWrapper.populateCalled);
}

TEST_F(GeminiTabHelperTest, TestGeneratePageContext_Timeout) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->SetContentsMimeType("text/html");
  web_state_->SetLoading(true);

  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  FakePageContextWrapper* fakeWrapper =
      [[FakePageContextWrapper alloc] initWithWebState:web_state_.get()
                                    completionCallback:base::DoNothing()];
  OCMStub([mockWrapperClass alloc]).andReturn(fakeWrapper);

  tab_helper_->GeneratePageContext(base::DoNothing());

  // Should NOT be called immediately.
  EXPECT_FALSE(fakeWrapper.populateCalled);

  // Fast forward by timeout.
  task_environment_.FastForwardBy(base::Seconds(3) + base::Milliseconds(100));

  EXPECT_TRUE(fakeWrapper.populateCalled);
}

// Tests that Gemini is available for a web state when the user is eligible and
// the web state is not off the record.
TEST_F(GeminiTabHelperTest, IsGeminiAvailableForWebState_WhenUserIsEligible) {
  web_state_->SetBrowserState(profile_.get());
  web_state_->SetCurrentURL(GURL("https://www.google.com"));
  web_state_->SetContentsMimeType("text/html");
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that Gemini is not available for a web state when the user is not
// eligible.
TEST_F(GeminiTabHelperTest,
       IsGeminiAvailableForWebState_WhenUserIsNotEligible) {
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  web_state_->WasShown();
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_FALSE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that Gemini is not available for a web state when the web state is off
// the record.
TEST_F(GeminiTabHelperTest,
       IsGeminiAvailableForWebState_WhenWebStateIsOffTheRecord) {
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_->GetOffTheRecordProfile());
  web_state_->WasShown();
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_FALSE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that Gemini is not available for a web state when the URL is an AIM
// URL.
TEST_F(GeminiTabHelperTest, IsGeminiAvailableForWebState_WhenUrlIsAimUrl) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kGeminiCopresence, kPageActionMenu},
      /*disabled_features=*/{});
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  web_state_->WasShown();
  web_state_->SetCurrentURL(
      GURL("https://www.google.com/search?q=test&udm=50"));
  web_state_->SetContentsMimeType("text/html");
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_FALSE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that Gemini is available for a web state when the URL is the Google
// home page.
TEST_F(GeminiTabHelperTest,
       IsGeminiAvailableForWebState_WhenUrlIsGoogleHomePage) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kGeminiCopresence, kPageActionMenu},
      /*disabled_features=*/{});
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  web_state_->WasShown();
  web_state_->SetCurrentURL(GURL("https://www.google.com"));
  web_state_->SetContentsMimeType("text/html");
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that Gemini is available for a web state when the URL is a Google
// Search URL but not an AIM URL.
TEST_F(GeminiTabHelperTest,
       IsGeminiAvailableForWebState_WhenUrlIsNotAimUrlButIsGoogleSearch) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kGeminiCopresence, kPageActionMenu},
      /*disabled_features=*/{});
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  web_state_->WasShown();
  web_state_->SetCurrentURL(GURL("https://www.google.com/search?q=test"));
  web_state_->SetContentsMimeType("text/html");
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that Gemini is available for a web state when the URL is not a Google
// Search URL and thus not an AIM URL.
TEST_F(GeminiTabHelperTest,
       IsGeminiAvailableForWebState_WhenUrlIsNotAimUrlAndNotGoogleSearch) {
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  web_state_->WasShown();
  web_state_->SetCurrentURL(GURL("https://www.example.com"));
  web_state_->SetContentsMimeType("text/html");
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that Gemini is not available for a web state when the URL is a PDF and
// the AllPages flag is disabled.
TEST_F(GeminiTabHelperTest,
       IsGeminiAvailableForWebState_WhenUrlIsPdf_AllPagesDisabled) {
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  web_state_->WasShown();
  web_state_->SetCurrentURL(GURL("https://www.example.com/test.pdf"));
  web_state_->SetContentsMimeType("application/pdf");
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_FALSE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that Gemini is available for a web state when the URL is a PDF and
// the AllPages flag is enabled.
TEST_F(GeminiTabHelperTest,
       IsGeminiAvailableForWebState_WhenUrlIsPdf_AllPagesEnabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kGeminiFloatyAllPages, kPageActionMenu},
      /*disabled_features=*/{});
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  web_state_->WasShown();
  web_state_->SetCurrentURL(GURL("https://www.example.com/test.pdf"));
  web_state_->SetContentsMimeType("application/pdf");
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests `IsGeminiAvailableForWebState` under default configuration (no flags).
TEST_F(GeminiTabHelperTest, IsGeminiAvailableForWebState_DefaultConfig) {
  web_state_->SetContentsMimeType("text/html");

  // Valid HTTPS URL should be available.
  web_state_->SetCurrentURL(GURL("https://www.example.com"));
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());

  // Valid HTTP URL should be available.
  web_state_->SetCurrentURL(GURL("http://www.example.com"));
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());

  // Invalid scheme (chrome) should not be available.
  web_state_->SetCurrentURL(GURL("chrome://settings"));
  EXPECT_FALSE(tab_helper_->IsGeminiAvailableForWebState());

  // Invalid scheme (about) should not be available.
  web_state_->SetCurrentURL(GURL("about:blank"));
  EXPECT_FALSE(tab_helper_->IsGeminiAvailableForWebState());

  // NTP should not be available.
  web_state_->SetCurrentURL(GURL(kChromeUINewTabURL));
  EXPECT_FALSE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests `IsGeminiAvailableForWebState` under Copresence configuration with SRP
// Check enabled.

// Tests `IsGeminiAvailableForWebState` under Copresence configuration.
TEST_F(GeminiTabHelperTest, IsGeminiAvailableForWebState_Copresence) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});

  web_state_->SetContentsMimeType("text/html");

  // Valid HTTPS URL should be available.
  web_state_->SetCurrentURL(GURL("https://www.example.com"));
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());

  // NTP should not be available.
  web_state_->SetCurrentURL(GURL(kChromeUINewTabURL));
  EXPECT_FALSE(tab_helper_->IsGeminiAvailableForWebState());

  // AIM URL should not be available.
  web_state_->SetCurrentURL(
      GURL("https://www.google.com/search?q=test&udm=50"));
  EXPECT_FALSE(tab_helper_->IsGeminiAvailableForWebState());

  // SRP URL (Google Home Page) should be available.
  web_state_->SetCurrentURL(GURL("https://www.google.com"));
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());

  // SRP URL (Google Search) should be available.
  web_state_->SetCurrentURL(GURL("https://www.google.com/search?q=test"));
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that Gemini is available for the NTP when the ChromeNextIa feature
// is enabled.
TEST_F(GeminiTabHelperTest,
       IsGeminiAvailableForWebState_WhenUrlIsNtp_ChromeNextIaEnabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kChromeNextIa, kPageActionMenu, kComposeboxIOS,
                            kComposeboxIpad},
      /*disabled_features=*/{});

  web_state_->SetBrowserState(profile_.get());
  web_state_->SetCurrentURL(GURL(kChromeUINewTabURL));
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_TRUE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that Gemini is not available for the NTP when the ChromeNextIa feature
// is disabled.
TEST_F(GeminiTabHelperTest,
       IsGeminiAvailableForWebState_WhenUrlIsNtp_ChromeNextIaDisabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu},
      /*disabled_features=*/{kChromeNextIa});

  web_state_->SetBrowserState(profile_.get());
  web_state_->SetCurrentURL(GURL(kChromeUINewTabURL));
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());
  EXPECT_FALSE(tab_helper_->IsGeminiAvailableForWebState());
}

// Tests that `GetCurrentPageType` correctly categorizes NTP URLs.
TEST_F(GeminiTabHelperTest, GetCurrentPageType_Ntp) {
  web_state_->SetCurrentURL(GURL(kChromeUINewTabURL));
  EXPECT_EQ(tab_helper_->GetCurrentPageType(),
            IOSGeminiInvocationPageType::kNewTabPage);
}

// Tests that `GetCurrentPageType` correctly categorizes Chrome internal URLs.
TEST_F(GeminiTabHelperTest, GetCurrentPageType_ChromeInternal) {
  web_state_->SetCurrentURL(GURL("chrome://settings"));
  EXPECT_EQ(tab_helper_->GetCurrentPageType(),
            IOSGeminiInvocationPageType::kChromeInternalOther);
}

// Tests that `GetCurrentPageType` correctly categorizes PDF documents.
TEST_F(GeminiTabHelperTest, GetCurrentPageType_Pdf) {
  web_state_->SetCurrentURL(GURL("https://www.example.com/test.pdf"));
  web_state_->SetContentsMimeType("application/pdf");
  EXPECT_EQ(tab_helper_->GetCurrentPageType(),
            IOSGeminiInvocationPageType::kPdfDocument);
}

// Tests that `GetCurrentPageType` correctly categorizes extractable HTML pages.
TEST_F(GeminiTabHelperTest, GetCurrentPageType_ExtractableWebPage_Html) {
  web_state_->SetCurrentURL(GURL("https://www.example.com"));
  web_state_->SetContentsMimeType("text/html");
  EXPECT_EQ(tab_helper_->GetCurrentPageType(),
            IOSGeminiInvocationPageType::kExtractableWebPage);
}

// Tests that `GetCurrentPageType` correctly categorizes extractable images.
TEST_F(GeminiTabHelperTest, GetCurrentPageType_ExtractableWebPage_Image) {
  web_state_->SetCurrentURL(GURL("https://www.example.com/image.png"));
  web_state_->SetContentsMimeType("image/png");
  EXPECT_EQ(tab_helper_->GetCurrentPageType(),
            IOSGeminiInvocationPageType::kExtractableWebPage);
}

// Tests that `GetCurrentPageType` correctly categorizes other non-extractable
// content.
TEST_F(GeminiTabHelperTest, GetCurrentPageType_Other) {
  web_state_->SetCurrentURL(GURL("https://www.example.com/file.bin"));
  web_state_->SetContentsMimeType("application/octet-stream");
  EXPECT_EQ(tab_helper_->GetCurrentPageType(),
            IOSGeminiInvocationPageType::kOtherNonExtractable);
}

// Tests that `GetPartialPageContext` returns a blocked state on the NTP.
TEST_F(GeminiTabHelperTest, GetPartialPageContext_Ntp) {
  web_state_->SetBrowserState(profile_.get());
  web_state_->SetCurrentURL(GURL(kChromeUINewTabURL));
  GeminiTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());

  struct TestResult {
    GeminiPageContext* context = nil;
  };
  TestResult result;
  tab_helper_->GeneratePageContext(base::BindRepeating(
      [](TestResult* tr, GeminiPageContext* response) {
        tr->context = response;
      },
      &result));

  ASSERT_NE(result.context, nil);
  EXPECT_EQ(result.context.geminiPageContextComputationState,
            ios::provider::GeminiPageContextComputationState::kBlocked);
}
