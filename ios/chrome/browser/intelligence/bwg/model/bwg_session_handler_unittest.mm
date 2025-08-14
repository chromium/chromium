// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_handler.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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
    AddWebState();
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
