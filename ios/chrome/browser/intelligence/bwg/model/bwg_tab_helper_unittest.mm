// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"

#import <memory>

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
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class BwgTabHelperTest : public PlatformTest {
 protected:
  BwgTabHelperTest()
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
    profile_ = std::move(builder).Build();
    profile_->GetPrefs()->SetInteger(prefs::kGeminiEnabledByPolicy, 0);
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    BwgTabHelper::CreateForWebState(web_state_.get());
    tab_helper_ = BwgTabHelper::FromWebState(web_state_.get());
    mock_bwg_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    tab_helper_->SetBwgCommandsHandler(mock_bwg_handler_);
    mock_location_bar_badge_handler_ =
        OCMProtocolMock(@protocol(LocationBarBadgeCommands));
    tab_helper_->SetLocationBarBadgeCommandsHandler(
        mock_location_bar_badge_handler_);
  }

  bool IsBwgUiShowing() { return tab_helper_->is_bwg_ui_showing_; }

  bool IsBwgSessionActiveInBackground() {
    return tab_helper_->is_bwg_session_active_in_background_;
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
  raw_ptr<BwgTabHelper, DanglingUntriaged> tab_helper_;

  // Mock BWG handler.
  id mock_bwg_handler_;
  // Mock Location Bar Badge handler.
  id mock_location_bar_badge_handler_;

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

  void SimulateFirstRunRecency(feature_engagement::Tracker* tracker, int days) {
    // Make first run not recent.
    tracker->NotifyEvent(feature_engagement::events::kIOSFirstRunComplete);
    task_environment_.FastForwardBy(base::Days(days));
    ForceFirstRunRecency(days);
  }
};

TEST_F(BwgTabHelperTest, TestSetBwgUiShowing) {
  ASSERT_FALSE(IsBwgUiShowing());
  tab_helper_->SetBwgUiShowing(true);
  ASSERT_TRUE(IsBwgUiShowing());
}

TEST_F(BwgTabHelperTest, TestContextualChipCommandSent) {
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

TEST_F(BwgTabHelperTest, TestGetIsBwgSessionActiveInBackground) {
  ASSERT_FALSE(tab_helper_->GetIsBwgSessionActiveInBackground());
  tab_helper_->SetBwgUiShowing(true);
  tab_helper_->WasHidden(web_state_.get());
  ASSERT_TRUE(tab_helper_->GetIsBwgSessionActiveInBackground());
}

TEST_F(BwgTabHelperTest, TestDeactivateBWGSession) {
  tab_helper_->SetBwgUiShowing(true);
  tab_helper_->WasHidden(web_state_.get());
  ASSERT_TRUE(IsBwgSessionActiveInBackground());
  // BWG is still considered as being shown in this case.
  ASSERT_TRUE(IsBwgUiShowing());

  tab_helper_->DeactivateBWGSession();
  ASSERT_FALSE(IsBwgSessionActiveInBackground());
  ASSERT_FALSE(IsBwgUiShowing());
}

TEST_F(BwgTabHelperTest, TestPrepareBwgFreBackgrounding) {
  ASSERT_FALSE(IsBwgSessionActiveInBackground());
  tab_helper_->PrepareBwgFreBackgrounding();
  ASSERT_TRUE(IsBwgSessionActiveInBackground());

  // Showing the UI should reset the background state.
  tab_helper_->SetBwgUiShowing(true);
  ASSERT_FALSE(IsBwgSessionActiveInBackground());
}

TEST_F(BwgTabHelperTest, TestIsLastInteractionUrlDifferent_NoLastInteraction) {
  ASSERT_TRUE(tab_helper_->IsLastInteractionUrlDifferent());
}

TEST_F(BwgTabHelperTest, TestIsLastInteractionUrlDifferent_SameURL) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu},
      /*disabled_features=*/{kGeminiCrossTab});
  GURL url("https://www.chromium.org");
  web_state_->SetCurrentURL(url);
  tab_helper_->CreateOrUpdateBwgSessionInStorage("server_id");
  ASSERT_FALSE(tab_helper_->IsLastInteractionUrlDifferent());
}

TEST_F(BwgTabHelperTest, TestIsLastInteractionUrlDifferent_DifferentURL) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu},
      /*disabled_features=*/{kGeminiCrossTab});
  GURL url1("https://www.chromium.org");
  web_state_->SetCurrentURL(url1);
  tab_helper_->CreateOrUpdateBwgSessionInStorage("server_id");

  GURL url2("https://www.google.com");
  web_state_->SetCurrentURL(url2);
  ASSERT_TRUE(tab_helper_->IsLastInteractionUrlDifferent());
}

TEST_F(BwgTabHelperTest,
       TestIsLastInteractionUrlDifferent_GeminiCrossTabEnabled_SameURL) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu, kGeminiCrossTab},
      /*disabled_features=*/{});
  GURL url("https://www.chromium.org");
  web_state_->SetCurrentURL(url);
  tab_helper_->CreateOrUpdateBwgSessionInStorage("server_id");
  ASSERT_FALSE(tab_helper_->IsLastInteractionUrlDifferent());
}

TEST_F(BwgTabHelperTest,
       TestIsLastInteractionUrlDifferent_GeminiCrossTabEnabled_DifferentURL) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu, kGeminiCrossTab},
      /*disabled_features=*/{});
  GURL url1("https://www.chromium.org");
  web_state_->SetCurrentURL(url1);
  tab_helper_->CreateOrUpdateBwgSessionInStorage("server_id");

  GURL url2("https://www.google.com");
  web_state_->SetCurrentURL(url2);
  ASSERT_TRUE(tab_helper_->IsLastInteractionUrlDifferent());
}

// TODO(crbug.com/430313339): Add a test for the last interaction case.
TEST_F(BwgTabHelperTest, TestShouldShowSuggestionChips) {
  web_state_->SetCurrentURL(GURL("https://www.google.com/search?q=test"));
  ASSERT_FALSE(tab_helper_->ShouldShowSuggestionChips());

  web_state_->SetCurrentURL(GURL("https://www.not-google.com"));
  ASSERT_TRUE(tab_helper_->ShouldShowSuggestionChips());
}

TEST_F(BwgTabHelperTest, TestCreateOrUpdateBwgSessionInStorage) {
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateBwgSessionInStorage(server_id);
  std::optional<std::string> retrieved_server_id = tab_helper_->GetServerId();
  ASSERT_TRUE(retrieved_server_id.has_value());
  ASSERT_EQ(server_id, retrieved_server_id.value());
}

TEST_F(BwgTabHelperTest, TestDeleteBwgSessionInStorage) {
  tab_helper_->CreateOrUpdateBwgSessionInStorage("test_server_id");
  ASSERT_TRUE(tab_helper_->GetServerId().has_value());
  tab_helper_->DeleteBwgSessionInStorage();
  ASSERT_FALSE(tab_helper_->GetServerId().has_value());
}

TEST_F(BwgTabHelperTest, TestGetServerId) {
  ASSERT_FALSE(tab_helper_->GetServerId().has_value());
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateBwgSessionInStorage(server_id);
  ASSERT_TRUE(tab_helper_->GetServerId().has_value());
  ASSERT_EQ(server_id, tab_helper_->GetServerId().value());
}

TEST_F(BwgTabHelperTest, TestGetServerId_Expired) {
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateBwgSessionInStorage(server_id);
  ASSERT_TRUE(tab_helper_->GetServerId().has_value());

  // Fast forward time to expire the session.
  task_environment_.FastForwardBy(BWGSessionValidityDuration() +
                                  base::Seconds(1));

  ASSERT_FALSE(tab_helper_->GetServerId().has_value());
}

TEST_F(BwgTabHelperTest, TestWasShown_RestoresSession) {
  OCMExpect([mock_bwg_handler_
      startBWGFlowWithEntryPoint:bwg::EntryPoint::TabReopen]);

  // Background a session and then show the tab.
  tab_helper_->PrepareBwgFreBackgrounding();
  tab_helper_->WasShown(web_state_.get());

  EXPECT_OCMOCK_VERIFY(mock_bwg_handler_);
}

TEST_F(BwgTabHelperTest, TestWasHidden_BackgroundsSession) {
  OCMExpect([mock_bwg_handler_ dismissBWGFlowWithCompletion:nil]);

  // Show the UI and then hide the tab.
  tab_helper_->SetBwgUiShowing(true);
  tab_helper_->WasHidden(web_state_.get());

  ASSERT_TRUE(IsBwgSessionActiveInBackground());
  EXPECT_OCMOCK_VERIFY(mock_bwg_handler_);
}

TEST_F(BwgTabHelperTest, TestDidStartNavigation_ShowsPromo) {
  feature_engagement::test::ScopedIphFeatureList iph_feature_list;
  iph_feature_list.InitAndEnableFeatures(
      {feature_engagement::kIPHiOSGeminiFullscreenPromoFeature, kPageActionMenu,
       kGeminiCrossTab, kGeminiNavigationPromo, kAskGeminiChip});

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

TEST_F(BwgTabHelperTest,
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

TEST_F(BwgTabHelperTest, TestDidStartNavigation_DoesNotShowPromoForNewUser) {
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

TEST_F(BwgTabHelperTest, TestDidStartNavigation_DoesNotShowPromoIfBWGStarted) {
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

TEST_F(BwgTabHelperTest, TestDidStartNavigation_ShowsPromoPrefs) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu, kGeminiCrossTab,
                            kGeminiNavigationPromo, kAskGeminiChip},
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

TEST_F(BwgTabHelperTest, TestDidStartNavigation_DoesNotShowPromoPrefs) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu, kGeminiCrossTab,
                            kGeminiNavigationPromo, kAskGeminiChip},
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

TEST_F(BwgTabHelperTest, WebStateDestroyed) {
  // Set some state.
  tab_helper_->SetBwgUiShowing(true);
  tab_helper_->PrepareBwgFreBackgrounding();

  // Destroy the webstate.
  web_state_.reset();

  // The test passes if it doesn't crash.
}

TEST_F(BwgTabHelperTest, WebStateDestroyed_CleansUpSession) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kGeminiCrossTab});
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateBwgSessionInStorage(server_id);
  ASSERT_EQ(tab_helper_->GetServerId().value(), server_id);

  // Destroy the webstate.
  web_state_.reset();

  // Create a new webstate and tab helper to check the prefs.
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  BwgTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = BwgTabHelper::FromWebState(web_state_.get());

  ASSERT_FALSE(tab_helper_->GetServerId().has_value());
}

TEST_F(BwgTabHelperTest,
       WebStateDestroyed_DoesNotCleanUpSession_GeminiCrossTabEnabled) {
  feature_list_.InitWithFeatures({kGeminiCrossTab, kPageActionMenu}, {});
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateBwgSessionInStorage(server_id);
  ASSERT_EQ(tab_helper_->GetServerId().value(), server_id);

  // Destroy the webstate.
  web_state_.reset();

  // Create a new webstate and tab helper to check the prefs.
  web_state_ = std::make_unique<web::FakeWebState>();
  web_state_->SetBrowserState(profile_.get());
  BwgTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = BwgTabHelper::FromWebState(web_state_.get());

  ASSERT_EQ(tab_helper_->GetServerId().value(), server_id);
}
