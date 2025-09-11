// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_handler.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
const base::TimeDelta kTestSessionDuration = base::Seconds(5);
const base::TimeDelta kTestResponseLatency = base::Milliseconds(500);
NSString* const kTestServerID = @"server_id";
}  // namespace

class BWGSessionHandlerTest : public PlatformTest {
 protected:
  BWGSessionHandlerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_list_ = browser_->GetWebStateList();
    session_handler_ =
        [[BWGSessionHandler alloc] initWithWebStateList:web_state_list_];

    // Set up mock handlers.
    mock_bwg_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    session_handler_.BWGHandler = mock_bwg_handler_;
    session_handler_.settingsHandler = mock_settings_handler_;

    AddWebState();
  }

  void TearDown() override {
    [mock_bwg_handler_ stopMocking];
    [mock_settings_handler_ stopMocking];
    PlatformTest::TearDown();
  }

  void AddWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    BwgTabHelper::CreateForWebState(web_state.get());
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
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  base::HistogramTester histogram_tester_;
  BWGSessionHandler* session_handler_;
  id mock_bwg_handler_;
  id mock_settings_handler_;
};

// Tests that UIDidDisappearWithClientID records the session duration.
TEST_F(BWGSessionHandlerTest, TestSessionDurationRecorded) {
  NSString* client_id = GetClientID();

  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];
  task_environment_.FastForwardBy(kTestSessionDuration);
  [session_handler_ UIDidDisappearWithClientID:client_id
                                      serverID:kTestServerID];

  histogram_tester_.ExpectTotalCount(kBWGSessionTimeHistogram, 1);
  histogram_tester_.ExpectTimeBucketCount(kBWGSessionTimeHistogram,
                                          kTestSessionDuration, 1);
}

// Tests that responseReceivedWithClientID records response latency.
TEST_F(BWGSessionHandlerTest, TestResponseLatencyRecorded) {
  NSString* client_id = GetClientID();

  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];
  [session_handler_ didSendQueryWithInputType:BWGInputTypeText
                          pageContextAttached:NO];
  task_environment_.FastForwardBy(kTestResponseLatency);
  [session_handler_ responseReceivedWithClientID:client_id
                                        serverID:kTestServerID];

  histogram_tester_.ExpectTotalCount(kResponseLatencyWithoutContextHistogram,
                                     1);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithoutContextHistogram, kTestResponseLatency, 1);
}

// Tests that didSendQueryWithInputType records the correct metrics.
TEST_F(BWGSessionHandlerTest, TestQueryMetricsRecorded) {
  [session_handler_ didSendQueryWithInputType:BWGInputTypeText
                          pageContextAttached:YES];

  histogram_tester_.ExpectUniqueSample(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kText, 1);
  histogram_tester_.ExpectUniqueSample(kPromptContextAttachmentHistogram, true,
                                       1);
}

// Tests that the first run flag is properly handled.
TEST_F(BWGSessionHandlerTest, TestFirstRunFlag) {
  NSString* client_id = GetClientID();

  // Set first run flag.
  web::WebState* web_state = web_state_list_->GetWebStateAt(0);
  BwgTabHelper* tab_helper = BwgTabHelper::FromWebState(web_state);
  tab_helper->SetIsFirstRun(true);

  [session_handler_ UIDidAppearWithClientID:client_id serverID:kTestServerID];
  [session_handler_ didSendQueryWithInputType:BWGInputTypeText
                          pageContextAttached:NO];
  task_environment_.FastForwardBy(kTestSessionDuration);
  [session_handler_ UIDidDisappearWithClientID:client_id
                                      serverID:kTestServerID];

  // Verify first run flag was cleared.
  EXPECT_FALSE(tab_helper->GetIsFirstRun());

  // Session metrics should reflect first session.
  histogram_tester_.ExpectTotalCount(kBWGSessionTimeHistogram, 1);
}

// Tests handling unrealized web states.
TEST_F(BWGSessionHandlerTest, TestUnrealizedWebStates) {
  // Add an unrealized web state.
  auto unrealized_web_state = std::make_unique<web::FakeWebState>();
  unrealized_web_state->SetBrowserState(profile_.get());
  unrealized_web_state->SetIsRealized(false);
  BwgTabHelper::CreateForWebState(unrealized_web_state.get());
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
TEST_F(BWGSessionHandlerTest, TestDifferentInputTypes) {
  // Test Summarize input type.
  BWGSessionHandler* handler1 =
      [[BWGSessionHandler alloc] initWithWebStateList:web_state_list_];
  [handler1 didSendQueryWithInputType:BWGInputTypeSummarize
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kSummarize, 1);

  // Test CheckThisSite input type.
  BWGSessionHandler* handler2 =
      [[BWGSessionHandler alloc] initWithWebStateList:web_state_list_];
  [handler2 didSendQueryWithInputType:BWGInputTypeCheckThisSite
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kCheckThisSite, 1);

  // Test FindRelatedSites input type.
  BWGSessionHandler* handler3 =
      [[BWGSessionHandler alloc] initWithWebStateList:web_state_list_];
  [handler3 didSendQueryWithInputType:BWGInputTypeFindRelatedSites
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kFindRelatedSites, 1);

  // Test AskAboutPage input type.
  BWGSessionHandler* handler4 =
      [[BWGSessionHandler alloc] initWithWebStateList:web_state_list_];
  [handler4 didSendQueryWithInputType:BWGInputTypeAskAboutPage
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kAskAboutPage, 1);

  // Test CreateFaq input type.
  BWGSessionHandler* handler5 =
      [[BWGSessionHandler alloc] initWithWebStateList:web_state_list_];
  [handler5 didSendQueryWithInputType:BWGInputTypeCreateFaq
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kCreateFaq, 1);

  // Test Unknown input type.
  BWGSessionHandler* handler6 =
      [[BWGSessionHandler alloc] initWithWebStateList:web_state_list_];
  [handler6 didSendQueryWithInputType:BWGInputTypeUnknown
                  pageContextAttached:NO];
  histogram_tester_.ExpectBucketCount(
      kFirstPromptSubmissionMethodHistogram,
      IOSGeminiFirstPromptSubmissionMethod::kUnknown, 1);
}
