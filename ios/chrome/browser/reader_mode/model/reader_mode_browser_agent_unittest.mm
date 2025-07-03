// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent.h"

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_test.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/page_side_swipe_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
constexpr char kTestURL[] = "https://www.example.com";
}  // namespace

// Test suite for ReaderModeBrowserAgent.
class ReaderModeBrowserAgentTest : public ReaderModeTest {
 public:
  void SetUp() override {
    ReaderModeTest::SetUp();

    test_browser_ = std::make_unique<TestBrowser>(profile());
    ReaderModeBrowserAgent::CreateForBrowser(test_browser_.get(),
                                             test_browser_->GetWebStateList());
    fake_reader_mode_handler_ =
        OCMStrictProtocolMock(@protocol(ReaderModeCommands));
    GetReaderModeBrowserAgent()->SetReaderModeHandler(
        fake_reader_mode_handler_);

    side_swipe_handler_ = OCMProtocolMock(@protocol(PageSideSwipeCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:side_swipe_handler_
                     forProtocol:@protocol(PageSideSwipeCommands)];

    // Initialize the WebStateList.
    InsertWebState();
    InsertWebState();
    InsertWebState();
    InsertWebState();
    GetWebStateList()->ActivateWebStateAt(0);

    EnableReaderMode(GetWebStateList()->GetWebStateAt(1));
    WaitForReaderModeContentReady();

    EnableReaderMode(GetWebStateList()->GetWebStateAt(3));
    WaitForReaderModeContentReady();
  }

  void TearDown() override {
    GetReaderModeBrowserAgent()->SetReaderModeHandler(nil);
  }

  // Inserts a FakeWebState with a ReaderModeTabHelper in the WebStateList.
  void InsertWebState() {
    std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
    GURL test_url = GURL(kTestURL);
    web_state->SetCurrentURL(test_url);
    SetReaderModeState(web_state.get(), test_url,
                       ReaderModeHeuristicResult::kReaderModeEligible,
                       "content");

    GetWebStateList()->InsertWebState(std::move(web_state));
  }

  // Returns the browser's WebStateList.
  WebStateList* GetWebStateList() { return test_browser_->GetWebStateList(); }

  // Returns the active WebState.
  web::WebState* GetActiveWebState() {
    return GetWebStateList()->GetActiveWebState();
  }

  // Returns the ReaderModeBrowserAgent.
  ReaderModeBrowserAgent* GetReaderModeBrowserAgent() {
    return ReaderModeBrowserAgent::FromBrowser(test_browser_.get());
  }

 protected:
  std::unique_ptr<TestBrowser> test_browser_;
  id fake_reader_mode_handler_;
  id side_swipe_handler_;
};

// Tests that the Reader mode UI is shown/dismissed when changing the current
// active WebState in the WebStateList.
TEST_F(ReaderModeBrowserAgentTest, ChangingActiveWebState) {
  OCMExpect([fake_reader_mode_handler_ showReaderMode]);
  GetWebStateList()->ActivateWebStateAt(1);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_handler_);

  OCMExpect([fake_reader_mode_handler_ hideReaderMode]);
  GetWebStateList()->ActivateWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_handler_);

  OCMExpect([fake_reader_mode_handler_ showReaderMode]);
  GetWebStateList()->ActivateWebStateAt(3);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_handler_);

  OCMExpect([fake_reader_mode_handler_ hideReaderMode]);
  GetWebStateList()->ActivateWebStateAt(WebStateList::kInvalidIndex);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_handler_);
}

// Tests that the Reader mode UI is not dismissed when moving the active
// WebState while the Reader mode UI is presented.
TEST_F(ReaderModeBrowserAgentTest, MovingActiveWebState) {
  OCMExpect([fake_reader_mode_handler_ showReaderMode]);
  GetWebStateList()->ActivateWebStateAt(1);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_handler_);

  // No call to `hideReaderMode` is expected.
  GetWebStateList()->MoveWebStateAt(1, 0);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_handler_);
}

// Tests that the Reader mode UI is shown/dismissed when Reader mode is
// activated/deactivated in the currently active WebState.
TEST_F(ReaderModeBrowserAgentTest, ChangingReaderModeStatus) {
  OCMExpect([fake_reader_mode_handler_ showReaderMode]);
  EnableReaderMode(GetActiveWebState());
  WaitForReaderModeContentReady();
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_handler_);

  OCMExpect([fake_reader_mode_handler_ hideReaderMode]);
  DisableReaderMode(GetActiveWebState());
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_handler_);
}
