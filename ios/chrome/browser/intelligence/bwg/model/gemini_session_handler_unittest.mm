// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_chat_message_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_chat_message_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace ios::provider {
void SetMockFeatureModeDisabledByQuota(bool disabled);
void SetMockRefillDateForFeatureMode(NSDate* date);
}  // namespace ios::provider

namespace {
const base::TimeDelta kTestSessionDuration = base::Seconds(5);
const base::TimeDelta kTestResponseLatency = base::Milliseconds(500);
NSString* const kTestServerID = @"server_id";

std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    ProfileIOS* profile) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}
}  // namespace

class GeminiSessionHandlerTest : public PlatformTest {
 protected:
  GeminiSessionHandlerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        kZeroStateSuggestionsCentralization);
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
    profile_ = std::move(builder).Build();
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_list_ = browser_->GetWebStateList();
    session_handler_ = [[GeminiSessionHandler alloc]
        initWithWebStateList:web_state_list_
                     tracker:mock_tracker_
                 prefService:profile_->GetPrefs()];
    optimization_guide_service_ =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());

    // Set up mock handlers.
    mock_gemini_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    session_handler_.geminiHandler = mock_gemini_handler_;
    session_handler_.settingsHandler = mock_settings_handler_;

    AddWebState();
  }

  void TearDown() override {
    [mock_gemini_handler_ stopMocking];
    [mock_settings_handler_ stopMocking];
    ios::provider::SetMockFeatureModeDisabledByQuota(false);
    ios::provider::SetMockRefillDateForFeatureMode(nil);
    PlatformTest::TearDown();
  }

  void AddWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    GeminiTabHelper::CreateForWebState(web_state.get());
    web_state_list_->InsertWebState(std::move(web_state),
                                    WebStateList::InsertionParams::Automatic());
  }

  NSString* GetClientID(int index = 0) {
    return
        [NSString stringWithFormat:@"%d", web_state_list_->GetWebStateAt(index)
                                              ->GetUniqueIdentifier()
                                              .identifier()];
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  GeminiSessionHandler* session_handler_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_;
  id mock_gemini_handler_;
  id mock_settings_handler_;
};

// Tests that UIDidDisappearWithClientID records the session duration.
TEST_F(GeminiSessionHandlerTest, TestSessionDurationRecorded) {
  NSString* client_id = GetClientID();

  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];
  task_environment_.FastForwardBy(kTestSessionDuration);
  [session_handler_ UIDidDisappearWithClientID:client_id
                                      serverID:kTestServerID];

  histogram_tester_.ExpectTotalCount(kGeminiSessionTimeHistogram, 1);
  histogram_tester_.ExpectTimeBucketCount(kGeminiSessionTimeHistogram,
                                          kTestSessionDuration, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("MobileGeminiSessionOpened"));
}

// Tests that responseReceivedWithClientID records response latency.
TEST_F(GeminiSessionHandlerTest, TestResponseLatencyRecorded) {
  NSString* client_id = GetClientID();

  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];
  [session_handler_ didSendQueryWithInputType:gemini::InputType::kText
                     isNanoBananaToolSelected:NO
                          imagesAttachedCount:0
                               longPressImage:NO
                          pageContextAttached:NO];
  task_environment_.FastForwardBy(kTestResponseLatency);
  [session_handler_ responseReceivedWithClientID:client_id
                                        serverID:kTestServerID
                        isNanoBananaToolSelected:NO
                                isImageGenerated:NO];

  histogram_tester_.ExpectTotalCount(kResponseLatencyWithoutContextHistogram,
                                     1);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithoutContextHistogram, kTestResponseLatency, 1);
}

// Tests that didSendQueryWithInputType records the correct metrics.
TEST_F(GeminiSessionHandlerTest, TestQueryMetricsRecorded) {
  [session_handler_ didSendQueryWithInputType:gemini::InputType::kText
                     isNanoBananaToolSelected:NO
                          imagesAttachedCount:0
                               longPressImage:NO
                          pageContextAttached:YES];

  histogram_tester_.ExpectUniqueSample(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kText, 1);
  histogram_tester_.ExpectUniqueSample(kPromptImageRemixEnabledHistogram, false,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPromptImagesAttachedCountHistogram, 0,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPromptLongPressImageIncludedHistogram,
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(kPromptContextAttachmentHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPromptChatContextAttachmentHistogram,
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(kPromptTabsAttachedCountHistogram, 1, 1);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("MobileGeminiChatPromptSent"));
}

// Tests that Nano Banana metrics are recorded correctly.
TEST_F(GeminiSessionHandlerTest, TestQueryMetricsRecorded_WithNanoBanana) {
  // Use a Nano Banana input type.
  [session_handler_
      didSendQueryWithInputType:gemini::InputType::
                                    kNanoBananaTurnThisPageIntoAComicStrip
       isNanoBananaToolSelected:YES
            imagesAttachedCount:1
                 longPressImage:YES
            pageContextAttached:NO];

  histogram_tester_.ExpectUniqueSample(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::
          kNanoBananaTurnThisPageIntoAComicStrip,
      1);

  // Check prompt metrics.
  histogram_tester_.ExpectUniqueSample(kPromptImageRemixEnabledHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPromptImagesAttachedCountHistogram, 1,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPromptLongPressImageIncludedHistogram,
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(kPromptContextAttachmentHistogram, false,
                                       1);
  histogram_tester_.ExpectUniqueSample(kPromptChatContextAttachmentHistogram,
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(kPromptTabsAttachedCountHistogram, 0, 1);
}

// Tests that generated image included in response is recorded.
TEST_F(GeminiSessionHandlerTest, TestResponseGeneratedImageRecorded) {
  NSString* client_id = GetClientID();
  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];
  [session_handler_ didSendQueryWithInputType:gemini::InputType::kText
                     isNanoBananaToolSelected:NO
                          imagesAttachedCount:0
                               longPressImage:NO
                          pageContextAttached:NO];

  [session_handler_ responseReceivedWithClientID:client_id
                                        serverID:kTestServerID
                        isNanoBananaToolSelected:NO
                                isImageGenerated:YES];

  histogram_tester_.ExpectUniqueSample(kResponseGeneratedImageIncluded, true,
                                       1);
}

// Tests that the first run flag is properly handled.
TEST_F(GeminiSessionHandlerTest, TestFirstRunFlag) {
  NSString* client_id = GetClientID();

  // Set first run flag.
  session_handler_.isFirstSession = YES;

  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];
  [session_handler_ didSendQueryWithInputType:gemini::InputType::kText
                     isNanoBananaToolSelected:NO
                          imagesAttachedCount:0
                               longPressImage:NO
                          pageContextAttached:NO];
  task_environment_.FastForwardBy(kTestSessionDuration);
  [session_handler_ UIDidDisappearWithClientID:client_id
                                      serverID:kTestServerID];

  // Session metrics should reflect first session.
  histogram_tester_.ExpectTotalCount(
      kGeminiSessionLengthFirstRunWithPromptHistogram, 1);
  histogram_tester_.ExpectTotalCount(kGeminiSessionTimeHistogram, 1);
}

// Tests handling unrealized web states.
TEST_F(GeminiSessionHandlerTest, TestUnrealizedWebStates) {
  // Add an unrealized web state.
  auto unrealized_web_state = std::make_unique<web::FakeWebState>();
  unrealized_web_state->SetBrowserState(profile_.get());
  unrealized_web_state->SetIsRealized(false);
  web_state_list_->InsertWebState(std::move(unrealized_web_state),
                                  WebStateList::InsertionParams::Automatic());

  // Add a realized web state.
  AddWebState();

  NSString* realized_client_id = GetClientID(2);

  [session_handler_ UIDidAppearWithClientID:realized_client_id
                                   serverID:kTestServerID];

  // The unrealized web state should not be affected.
  web::WebState* unrealized = web_state_list_->GetWebStateAt(1);
  EXPECT_FALSE(unrealized->IsRealized());
}

// Tests different input types for first prompt submission.
TEST_F(GeminiSessionHandlerTest, TestDifferentInputTypes) {
  // Test Summarize input type.
  GeminiSessionHandler* handler1 =
      [[GeminiSessionHandler alloc] initWithWebStateList:web_state_list_
                                                 tracker:mock_tracker_
                                             prefService:profile_->GetPrefs()];
  [handler1 didSendQueryWithInputType:gemini::InputType::kSummarize
             isNanoBananaToolSelected:NO
                  imagesAttachedCount:0
                       longPressImage:NO
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kSummarize, 1);

  // Test CheckThisSite input type.
  GeminiSessionHandler* handler2 =
      [[GeminiSessionHandler alloc] initWithWebStateList:web_state_list_
                                                 tracker:mock_tracker_
                                             prefService:profile_->GetPrefs()];
  [handler2 didSendQueryWithInputType:gemini::InputType::kCheckThisSite
             isNanoBananaToolSelected:NO
                  imagesAttachedCount:0
                       longPressImage:NO
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kCheckThisSite, 1);

  // Test FindRelatedSites input type.
  GeminiSessionHandler* handler3 =
      [[GeminiSessionHandler alloc] initWithWebStateList:web_state_list_
                                                 tracker:mock_tracker_
                                             prefService:profile_->GetPrefs()];
  [handler3 didSendQueryWithInputType:gemini::InputType::kFindRelatedSites
             isNanoBananaToolSelected:NO
                  imagesAttachedCount:0
                       longPressImage:NO
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kFindRelatedSites, 1);

  // Test AskAboutPage input type.
  GeminiSessionHandler* handler4 =
      [[GeminiSessionHandler alloc] initWithWebStateList:web_state_list_
                                                 tracker:mock_tracker_
                                             prefService:profile_->GetPrefs()];
  [handler4 didSendQueryWithInputType:gemini::InputType::kAskAboutPage
             isNanoBananaToolSelected:NO
                  imagesAttachedCount:0
                       longPressImage:NO
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kAskAboutPage, 1);

  // Test CreateFaq input type.
  GeminiSessionHandler* handler5 =
      [[GeminiSessionHandler alloc] initWithWebStateList:web_state_list_
                                                 tracker:mock_tracker_
                                             prefService:profile_->GetPrefs()];
  [handler5 didSendQueryWithInputType:gemini::InputType::kCreateFaq
             isNanoBananaToolSelected:NO
                  imagesAttachedCount:0
                       longPressImage:NO
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kCreateFaq, 1);

  // Test Unknown input type.
  GeminiSessionHandler* handler6 =
      [[GeminiSessionHandler alloc] initWithWebStateList:web_state_list_
                                                 tracker:mock_tracker_
                                             prefService:profile_->GetPrefs()];
  [handler6 didSendQueryWithInputType:gemini::InputType::kUnknown
             isNanoBananaToolSelected:NO
                  imagesAttachedCount:0
                       longPressImage:NO
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kUnknown, 1);
}

// Tests that updateSessionWithClientID handles invalid client IDs gracefully.
TEST_F(GeminiSessionHandlerTest, TestUpdateSessionWithInvalidClientID) {
  NSString* invalid_client_id = @"invalid_id_999";
  NSString* server_id = @"test_server_123";

  // This should not crash even with invalid client ID.
  EXPECT_NO_FATAL_FAILURE([session_handler_
      newSessionCreatedWithClientID:invalid_client_id
                           serverID:server_id]);
}

// Tests that UIDidAppearWithClientID sets the session to active state.
TEST_F(GeminiSessionHandlerTest, TestSetSessionActiveTrue) {
  NSString* client_id = GetClientID();

  // Set session active.
  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];

  // Get the tab helper and verify it exists.
  web::WebState* web_state = web_state_list_->GetWebStateAt(0);
  GeminiTabHelper* tab_helper = GeminiTabHelper::FromWebState(web_state);
  ASSERT_TRUE(tab_helper);
}

// Tests that methods handle missing web states gracefully.
TEST_F(GeminiSessionHandlerTest, TestWebStateWithClientIDNotFound) {
  NSString* non_existent_id = @"999999999";

  // Test that methods handle missing web state gracefully.
  EXPECT_NO_FATAL_FAILURE([session_handler_
      UIDidAppearWithClientID:non_existent_id
                     serverID:kTestServerID]);
  EXPECT_NO_FATAL_FAILURE([session_handler_
      UIDidDisappearWithClientID:non_existent_id
                        serverID:kTestServerID]);
  EXPECT_NO_FATAL_FAILURE([session_handler_
      responseReceivedWithClientID:non_existent_id
                          serverID:kTestServerID
          isNanoBananaToolSelected:NO
                  isImageGenerated:NO]);
}

// Tests that updateSessionWithClientID creates/updates the session in storage.
TEST_F(GeminiSessionHandlerTest, TestUpdateSessionWithClientID) {
  NSString* client_id = GetClientID();
  NSString* server_id = @"test_server_id";

  web_state_list_->ActivateWebStateAt(0);

  // Check initial state - no server ID should exist.
  std::optional<std::string> initial_server_id =
      gemini::GetConversationId(profile_->GetPrefs());
  EXPECT_FALSE(initial_server_id.has_value());

  [session_handler_ UIDidAppearWithClientID:client_id serverID:server_id];

  // Verify server ID was stored correctly.
  std::optional<std::string> stored_server_id =
      gemini::GetConversationId(profile_->GetPrefs());
  EXPECT_TRUE(stored_server_id.has_value());
  EXPECT_EQ(stored_server_id.value(), "test_server_id");
}

// Tests that didTapNewChatButtonWithSessionID maintains client ID consistency.
TEST_F(GeminiSessionHandlerTest, TestNewChatButtonTapped) {
  NSString* client_id = GetClientID();
  NSString* conversation_id = @"conversation_123";
  NSString* server_id = @"test_server_123";

  web_state_list_->ActivateWebStateAt(0);

  // Create a session with stored server ID.
  [session_handler_ UIDidAppearWithClientID:client_id serverID:server_id];

  web::WebState* web_state = web_state_list_->GetWebStateAt(0);
  GeminiTabHelper* tab_helper = GeminiTabHelper::FromWebState(web_state);

  // Verify session exists with server ID.
  std::optional<std::string> initial_server_id =
      gemini::GetConversationId(profile_->GetPrefs());
  EXPECT_TRUE(initial_server_id.has_value());
  EXPECT_EQ(initial_server_id.value(), "test_server_123");

  // Verify client ID exists.
  std::string initial_client_id = tab_helper->GetClientId();
  EXPECT_FALSE(initial_client_id.empty());

  // Tap new chat button.
  [session_handler_ didTapNewChatButtonWithSessionID:client_id
                                      conversationID:conversation_id];

  // Verify client ID remains unchanged after new chat.
  std::string post_delete_client_id = tab_helper->GetClientId();
  EXPECT_EQ(initial_client_id, post_delete_client_id);
}

// Tests that didTapNewChatButtonWithSessionID resets prompt counters and flags.
TEST_F(GeminiSessionHandlerTest, TestNewChatButtonResetsFlags) {
  NSString* client_id = GetClientID();
  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];
  [session_handler_ didSendQueryWithInputType:gemini::InputType::kText
                     isNanoBananaToolSelected:NO
                          imagesAttachedCount:0
                               longPressImage:NO
                          pageContextAttached:NO];

  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kText, 1);
  histogram_tester_.ExpectBucketCount(
      kPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kText, 1);

  // Tap new chat button.
  [session_handler_ didTapNewChatButtonWithSessionID:client_id
                                      conversationID:@"conversation_123"];

  // Now send another query and check that it logs as the first prompt again.
  [session_handler_ didSendQueryWithInputType:gemini::InputType::kText
                     isNanoBananaToolSelected:NO
                          imagesAttachedCount:0
                               longPressImage:NO
                          pageContextAttached:NO];

  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kText, 2);
  histogram_tester_.ExpectBucketCount(
      kPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kText, 2);
}

// Tests that didTapNewChatButtonWithSessionID calls didTapNewChatButton on
// geminiViewStateDelegate.
TEST_F(GeminiSessionHandlerTest, TestNewChatButtonNotifiesViewStateDelegate) {
  id mock_delegate = OCMProtocolMock(@protocol(GeminiViewStateDelegate));
  session_handler_.geminiViewStateDelegate = mock_delegate;

  OCMExpect([mock_delegate didTapNewChatButton]);

  [session_handler_ didTapNewChatButtonWithSessionID:@"session_123"
                                      conversationID:@"conv_123"];

  [mock_delegate verify];
}

// Tests that responseCancelledWithReason notifies geminiViewStateDelegate.
TEST_F(GeminiSessionHandlerTest,
       TestResponseCancelledNotifiesViewStateDelegate) {
  id mock_delegate = OCMProtocolMock(@protocol(GeminiViewStateDelegate));
  session_handler_.geminiViewStateDelegate = mock_delegate;

  OCMExpect([mock_delegate
      responseCancelledWithReason:GeminiCancelTypeStopButtonTapped]);

  [session_handler_ responseCancelledWithReason:GeminiCancelTypeStopButtonTapped
                                      sessionID:@"session_123"
                                 conversationID:@"conv_123"];

  [mock_delegate verify];
}

// Tests that didTapFeedbackButton records the correct metrics.
TEST_F(GeminiSessionHandlerTest, TestFeedbackMetricsRecorded) {
  // Test Thumbs Up.
  [session_handler_ didTapFeedbackButton:GeminiFeedbackType::kThumbsUp
                               sessionID:kTestServerID
                          conversationID:kTestServerID];
  histogram_tester_.ExpectBucketCount(kFeedbackHistogram,
                                      IOSGeminiFeedback::kThumbsUp, 1);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("MobileGeminiFeedbackThumbsUp"));

  // Test Thumbs Down.
  [session_handler_ didTapFeedbackButton:GeminiFeedbackType::kThumbsDown
                               sessionID:kTestServerID
                          conversationID:kTestServerID];
  histogram_tester_.ExpectBucketCount(kFeedbackHistogram,
                                      IOSGeminiFeedback::kThumbsDown, 1);
  EXPECT_EQ(
      1, user_action_tester_.GetActionCount("MobileGeminiFeedbackThumbsDown"));
}

// Tests that didSendQueryWithInputType notifies the tracker when What Can
// Gemini Do is tapped.
TEST_F(GeminiSessionHandlerTest,
       OnSuggestionTappedWhatCanGeminiDoNotifiesTracker) {
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(feature_engagement::events::kIOSGeminiWhatCanGeminiDoTapped))
      .Times(1);

  [session_handler_
      didSendQueryWithInputType:gemini::InputType::kWhatCanGeminiDo
       isNanoBananaToolSelected:NO
            imagesAttachedCount:0
                 longPressImage:NO
            pageContextAttached:NO];
}

// Tests that didSendQueryWithInputType does not notify the tracker for other
// suggestion types.
TEST_F(GeminiSessionHandlerTest,
       OnSuggestionTappedOtherTypesDoNotNotifyTracker) {
  EXPECT_CALL(*mock_tracker_, NotifyEvent(testing::_)).Times(0);

  [session_handler_ didSendQueryWithInputType:gemini::InputType::kText
                     isNanoBananaToolSelected:NO
                          imagesAttachedCount:0
                               longPressImage:NO
                          pageContextAttached:NO];
}

// Tests that showConsentScreenWithCompletion switches view mode to kFloaty if
// rejected.
TEST_F(GeminiSessionHandlerTest,
       TestShowConsentScreenRejectionSwitchesToFloaty) {
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kLive);

  OCMExpect([mock_gemini_handler_
                startGeminiLiveFirstRunWithBaseViewController:[OCMArg any]
                                                   completion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion)(BOOL success);
        [invocation getArgument:&completion atIndex:3];
        if (completion) {
          completion(NO);
        }
      });

  __block BOOL callback_invoked = NO;
  [session_handler_ geminiLive:nil
      showConsentScreenWithCompletion:^(BOOL accepted) {
        EXPECT_FALSE(accepted);
        callback_invoked = YES;
      }];

  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kFloaty);
  [mock_gemini_handler_ verify];
}

// Tests that receiving a response logs the quota reached metric when quota is
// exhausted, and deduplicates subsequent responses for the same refill date.
TEST_F(GeminiSessionHandlerTest, TestQuotaReachedMetricRecordedOnResponse) {
  base::UserActionTester user_action_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kPageActionMenu, kGeminiAureus}, {});

  ios::provider::SetMockFeatureModeDisabledByQuota(true);
  NSDate* refill_date = [NSDate dateWithTimeIntervalSinceNow:3600];
  ios::provider::SetMockRefillDateForFeatureMode(refill_date);

  // First response logs the metric.
  [session_handler_ responseReceivedWithClientID:GetClientID()
                                        serverID:kTestServerID
                        isNanoBananaToolSelected:NO
                                isImageGenerated:NO];
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileGeminiQuotaReached"));

  // Subsequent response for the same refill date is deduplicated.
  [session_handler_ responseReceivedWithClientID:GetClientID()
                                        serverID:kTestServerID
                        isNanoBananaToolSelected:NO
                                isImageGenerated:NO];
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileGeminiQuotaReached"));
}

// Tests that transitioning to `kThinking` in Live mode records first prompt and
// prompt submission method histograms, and increments session prompt count.
TEST_F(GeminiSessionHandlerTest, TestLiveSessionThinkingRecordsPrompt) {
  NSString* client_id = GetClientID();
  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];

  // Transition to thinking.
  [session_handler_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kThinking
                      sessionID:client_id
                 conversationID:kTestServerID];

  histogram_tester_.ExpectUniqueSample(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 1);
  histogram_tester_.ExpectUniqueSample(
      kPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 1);

  // Consecutive thinking status does not record another prompt.
  [session_handler_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kThinking
                      sessionID:client_id
                 conversationID:kTestServerID];
  histogram_tester_.ExpectBucketCount(
      kPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 1);

  // Transition out and back into thinking (a second prompt).
  [session_handler_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kResponding
                      sessionID:client_id
                 conversationID:kTestServerID];
  [session_handler_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kListening
                      sessionID:client_id
                 conversationID:kTestServerID];
  [session_handler_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kThinking
                      sessionID:client_id
                 conversationID:kTestServerID];

  // First prompt submission should not be recorded again.
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 1);
  // Prompt submission method should be recorded twice.
  histogram_tester_.ExpectBucketCount(
      kPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 2);

  // End the session and verify it is classified as with-prompt, not abandoned.
  task_environment_.FastForwardBy(kTestSessionDuration);
  [session_handler_ UIDidDisappearWithClientID:client_id
                                      serverID:kTestServerID];

  histogram_tester_.ExpectTotalCount(kGeminiSessionLengthWithPromptHistogram,
                                     1);
  histogram_tester_.ExpectTotalCount(kGeminiSessionLengthAbandonedHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kSessionPromptCountHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(kSessionFirstPromptHistogram, true, 1);
}

// Tests that
// `didUpdateProcessingStatus:dormantReason:sessionID:conversationID:` also
// updates processing status and records prompts on transition to thinking.
TEST_F(GeminiSessionHandlerTest, TestLiveSessionDormantReasonRecordsPrompt) {
  NSString* client_id = GetClientID();
  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];

  [session_handler_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kThinking
                  dormantReason:ios::provider::GeminiDormantReason::kUnknown
                      sessionID:client_id
                 conversationID:kTestServerID];

  histogram_tester_.ExpectUniqueSample(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 1);
  histogram_tester_.ExpectUniqueSample(
      kPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 1);

  [session_handler_ UIDidDisappearWithClientID:client_id
                                      serverID:kTestServerID];

  histogram_tester_.ExpectTotalCount(kGeminiSessionLengthWithPromptHistogram,
                                     1);
  histogram_tester_.ExpectTotalCount(kGeminiSessionLengthAbandonedHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kSessionPromptCountHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kSessionFirstPromptHistogram, true, 1);
}

// Tests that a session without prompts is classified as abandoned.
TEST_F(GeminiSessionHandlerTest, TestAbandonedSessionWithoutPrompts) {
  NSString* client_id = GetClientID();
  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];

  // Client enters listening state without reaching thinking.
  [session_handler_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kListening
                      sessionID:client_id
                 conversationID:kTestServerID];

  task_environment_.FastForwardBy(kTestSessionDuration);
  [session_handler_ UIDidDisappearWithClientID:client_id
                                      serverID:kTestServerID];

  histogram_tester_.ExpectTotalCount(kGeminiSessionLengthWithPromptHistogram,
                                     0);
  histogram_tester_.ExpectTotalCount(kGeminiSessionLengthAbandonedHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kSessionPromptCountHistogram, 0, 1);
  histogram_tester_.ExpectUniqueSample(kSessionFirstPromptHistogram, false, 1);
}

// Tests that a session with both text and live prompts accumulates the prompt
// count correctly and attributes the first prompt method to the initial chat
// prompt.
TEST_F(GeminiSessionHandlerTest, TestMixedChatAndLivePromptsInSession) {
  NSString* client_id = GetClientID();
  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];

  // Send a text prompt first.
  [session_handler_ didSendQueryWithInputType:gemini::InputType::kText
                     isNanoBananaToolSelected:NO
                          imagesAttachedCount:0
                               longPressImage:NO
                          pageContextAttached:NO];

  histogram_tester_.ExpectUniqueSample(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kText, 1);
  histogram_tester_.ExpectBucketCount(
      kPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kText, 1);

  // Live turn transitions to thinking.
  [session_handler_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kThinking
                      sessionID:client_id
                 conversationID:kTestServerID];

  // First prompt submission method remains kText.
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 0);
  // Prompt submission method now has both kText and kLivePrompt.
  histogram_tester_.ExpectBucketCount(
      kPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 1);

  [session_handler_ UIDidDisappearWithClientID:client_id
                                      serverID:kTestServerID];

  histogram_tester_.ExpectTotalCount(kGeminiSessionLengthWithPromptHistogram,
                                     1);
  histogram_tester_.ExpectTotalCount(kGeminiSessionLengthAbandonedHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kSessionPromptCountHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(kSessionFirstPromptHistogram, true, 1);
}

// Tests that tapping new chat resets the live processing status so subsequent
// thinking transitions are treated as the new session's first prompt.
TEST_F(GeminiSessionHandlerTest, TestNewChatResetsLiveProcessingStatus) {
  NSString* client_id = GetClientID();
  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];

  [session_handler_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kThinking
                      sessionID:client_id
                 conversationID:kTestServerID];

  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 1);

  // Tap new chat button.
  [session_handler_ didTapNewChatButtonWithSessionID:client_id
                                      conversationID:@"new_conv"];

  // Transition to thinking again in the new chat.
  [session_handler_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kThinking
                      sessionID:client_id
                 conversationID:@"new_conv"];

  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kLivePrompt, 2);

  [session_handler_ UIDidDisappearWithClientID:client_id
                                      serverID:kTestServerID];

  histogram_tester_.ExpectUniqueSample(kSessionPromptCountHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kSessionFirstPromptHistogram, true, 1);
}

// Test that chat messages are sent when no chat message handler is set.
TEST_F(GeminiSessionHandlerTest, TestHandleChatMessageRequestWithoutHandler) {
  GeminiChatMessageRequest* request =
      [[GeminiChatMessageRequest alloc] initWithText:@"Hello"
                                           sessionID:GetClientID()
                                      conversationID:kTestServerID];
  __block int completion_count = 0;
  __block GeminiChatMessageResponse* received_response = nil;
  [session_handler_
      handleChatMessageRequest:request
                    completion:^(GeminiChatMessageResponse* response) {
                      ++completion_count;
                      received_response = response;
                    }];
  EXPECT_EQ(1, completion_count);
  ASSERT_TRUE(received_response);
  EXPECT_FALSE(received_response.shouldConsume);
}

// Test that the request is forwarded unchanged to the chat message handler and
// that a consuming response is passed back unchanged.
TEST_F(GeminiSessionHandlerTest, TestHandleChatMessageRequestForwardsConsumed) {
  GeminiChatMessageRequest* request =
      [[GeminiChatMessageRequest alloc] initWithText:@"Hello"
                                           sessionID:GetClientID()
                                      conversationID:nil];
  GeminiChatMessageResponse* handler_response =
      [[GeminiChatMessageResponse alloc] initWithShouldConsume:YES];
  id mock_handler = OCMProtocolMock(@protocol(GeminiChatMessageHandler));
  id invoke_completion = [OCMArg invokeBlockWithArgs:handler_response, nil];
  OCMExpect([mock_handler handleChatMessageRequest:request
                                        completion:invoke_completion]);
  session_handler_.chatMessageHandler = mock_handler;

  __block int completion_count = 0;
  __block GeminiChatMessageResponse* received_response = nil;
  [session_handler_
      handleChatMessageRequest:request
                    completion:^(GeminiChatMessageResponse* response) {
                      ++completion_count;
                      received_response = response;
                    }];
  EXPECT_EQ(1, completion_count);
  EXPECT_EQ(handler_response, received_response);
  EXPECT_TRUE(received_response.shouldConsume);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Test that a non-consuming response from the chat message handler is passed
// back unchanged, so a hard-coded consuming answer would be caught.
TEST_F(GeminiSessionHandlerTest,
       TestHandleChatMessageRequestForwardsNotConsumed) {
  GeminiChatMessageRequest* request =
      [[GeminiChatMessageRequest alloc] initWithText:@"Hello"
                                           sessionID:GetClientID()
                                      conversationID:kTestServerID];
  GeminiChatMessageResponse* handler_response =
      [[GeminiChatMessageResponse alloc] initWithShouldConsume:NO];
  id mock_handler = OCMProtocolMock(@protocol(GeminiChatMessageHandler));
  id invoke_completion = [OCMArg invokeBlockWithArgs:handler_response, nil];
  OCMExpect([mock_handler handleChatMessageRequest:request
                                        completion:invoke_completion]);
  session_handler_.chatMessageHandler = mock_handler;

  __block int completion_count = 0;
  __block GeminiChatMessageResponse* received_response = nil;
  [session_handler_
      handleChatMessageRequest:request
                    completion:^(GeminiChatMessageResponse* response) {
                      ++completion_count;
                      received_response = response;
                    }];
  EXPECT_EQ(1, completion_count);
  EXPECT_EQ(handler_response, received_response);
  EXPECT_FALSE(received_response.shouldConsume);
  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that `didFinishLoadingConversationWithID:error:` forwards the call to
// `geminiViewStateDelegate`.
TEST_F(GeminiSessionHandlerTest,
       TestDidFinishLoadingConversationNotifiesViewStateDelegate) {
  id mock_delegate = OCMProtocolMock(@protocol(GeminiViewStateDelegate));
  session_handler_.geminiViewStateDelegate = mock_delegate;
  NSError* error = [NSError errorWithDomain:@"TestDomain" code:1 userInfo:nil];

  OCMExpect([mock_delegate didFinishLoadingConversationWithID:@"conv_123"
                                                        error:error]);

  [session_handler_ didFinishLoadingConversationWithID:@"conv_123" error:error];

  [mock_delegate verify];
}
