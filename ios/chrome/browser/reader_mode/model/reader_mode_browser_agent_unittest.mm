// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent.h"

#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent_delegate.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_test.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_side_swipe_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_chip_commands.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
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
    TabsDependencyInstallerManager::CreateForBrowser(test_browser_.get());
    ReaderModeBrowserAgent::CreateForBrowser(test_browser_.get());

    delegate_ = OCMProtocolMock(@protocol(ReaderModeBrowserAgentDelegate));
    GetReaderModeBrowserAgent()->SetDelegate(delegate_);

    side_swipe_handler_ = OCMProtocolMock(@protocol(PageSideSwipeCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:side_swipe_handler_
                     forProtocol:@protocol(PageSideSwipeCommands)];

    fake_reader_mode_chip_handler_ =
        OCMProtocolMock(@protocol(ReaderModeChipCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:fake_reader_mode_chip_handler_
                     forProtocol:@protocol(ReaderModeChipCommands)];

    contextual_panel_entrypoint_handler_ =
        OCMProtocolMock(@protocol(ContextualPanelEntrypointCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:contextual_panel_entrypoint_handler_
                     forProtocol:@protocol(ContextualPanelEntrypointCommands)];

    // Initialize the WebStateList.
    InsertWebState();
    InsertWebState();
    InsertWebState();
    InsertWebState();
    GetWebStateList()->ActivateWebStateAt(0);

    EnableReaderMode(GetWebStateList()->GetWebStateAt(1),
                     ReaderModeAccessPoint::kContextualChip);
    WaitForAvailableReaderModeContentInWebState(
        GetWebStateList()->GetWebStateAt(1));

    EnableReaderMode(GetWebStateList()->GetWebStateAt(3),
                     ReaderModeAccessPoint::kContextualChip);
    WaitForAvailableReaderModeContentInWebState(
        GetWebStateList()->GetWebStateAt(3));
  }

  void TearDown() override { GetReaderModeBrowserAgent()->SetDelegate(nil); }

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

  // Activates a web state that expects Reader mode to be displayed.
  void ActivateWebStateWithReaderModeAt(int index) {
    GetWebStateList()->ActivateWebStateAt(index);
    WaitForAvailableReaderModeContentInWebState(
        GetWebStateList()->GetWebStateAt(index));
  }

 protected:
  std::unique_ptr<TestBrowser> test_browser_;
  id fake_reader_mode_chip_handler_;
  id side_swipe_handler_;
  id contextual_panel_entrypoint_handler_;
  id delegate_;
};

// Tests that the Reader mode UI is shown/dismissed when changing the current
// active WebState in the WebStateList.
TEST_F(ReaderModeBrowserAgentTest, ChangingActiveWebState) {
  OCMExpect([delegate_ readerModeBrowserAgent:GetReaderModeBrowserAgent()
                          showContentAnimated:NO]);
  OCMExpect([fake_reader_mode_chip_handler_ showReaderModeChip]);
  OCMExpect([contextual_panel_entrypoint_handler_
      cancelContextualPanelEntrypointLoudMoment]);
  ActivateWebStateWithReaderModeAt(1);
  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_chip_handler_);
  EXPECT_OCMOCK_VERIFY(contextual_panel_entrypoint_handler_);

  OCMExpect([delegate_ readerModeBrowserAgent:GetReaderModeBrowserAgent()
                          hideContentAnimated:NO]);
  OCMExpect([fake_reader_mode_chip_handler_ hideReaderModeChip]);
  GetWebStateList()->ActivateWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_chip_handler_);

  OCMExpect([delegate_ readerModeBrowserAgent:GetReaderModeBrowserAgent()
                          showContentAnimated:NO]);
  OCMExpect([fake_reader_mode_chip_handler_ showReaderModeChip]);
  OCMExpect([contextual_panel_entrypoint_handler_
      cancelContextualPanelEntrypointLoudMoment]);
  ActivateWebStateWithReaderModeAt(3);
  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_chip_handler_);
  EXPECT_OCMOCK_VERIFY(contextual_panel_entrypoint_handler_);

  OCMExpect([delegate_ readerModeBrowserAgent:GetReaderModeBrowserAgent()
                          hideContentAnimated:NO]);
  OCMExpect([fake_reader_mode_chip_handler_ hideReaderModeChip]);
  GetWebStateList()->ActivateWebStateAt(WebStateList::kInvalidIndex);
  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_chip_handler_);
}

// Tests that the Reader mode UI is not dismissed when moving the active
// WebState while the Reader mode UI is presented.
TEST_F(ReaderModeBrowserAgentTest, MovingActiveWebState) {
  OCMExpect([delegate_ readerModeBrowserAgent:GetReaderModeBrowserAgent()
                          showContentAnimated:NO]);
  OCMExpect([fake_reader_mode_chip_handler_ showReaderModeChip]);
  OCMExpect([contextual_panel_entrypoint_handler_
      cancelContextualPanelEntrypointLoudMoment]);
  ActivateWebStateWithReaderModeAt(1);
  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_chip_handler_);
  EXPECT_OCMOCK_VERIFY(contextual_panel_entrypoint_handler_);

  // No call to `hideReaderMode` is expected.
  GetWebStateList()->MoveWebStateAt(1, 0);
  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_chip_handler_);
  EXPECT_OCMOCK_VERIFY(contextual_panel_entrypoint_handler_);
}

// Tests that the Reader mode UI is shown/dismissed when Reader mode is
// activated/deactivated in the currently active WebState.
TEST_F(ReaderModeBrowserAgentTest, ChangingReaderModeStatus) {
  OCMExpect([delegate_ readerModeBrowserAgent:GetReaderModeBrowserAgent()
                          showContentAnimated:YES]);
  OCMExpect([fake_reader_mode_chip_handler_ showReaderModeChip]);
  OCMExpect([contextual_panel_entrypoint_handler_
      cancelContextualPanelEntrypointLoudMoment]);
  EnableReaderMode(GetActiveWebState(), ReaderModeAccessPoint::kContextualChip);
  WaitForAvailableReaderModeContentInWebState(GetActiveWebState());
  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_chip_handler_);
  EXPECT_OCMOCK_VERIFY(contextual_panel_entrypoint_handler_);

  OCMExpect([delegate_ readerModeBrowserAgent:GetReaderModeBrowserAgent()
                          hideContentAnimated:YES]);
  OCMExpect([fake_reader_mode_chip_handler_ hideReaderModeChip]);
  DisableReaderMode(GetActiveWebState());
  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_chip_handler_);
}

// Tests that the Reader mode UI is hidden when a navigation occurs in the
// active WebState.
TEST_F(ReaderModeBrowserAgentTest, NavigationInActiveWebState) {
  // Show reader mode.
  OCMExpect([delegate_ readerModeBrowserAgent:GetReaderModeBrowserAgent()
                          showContentAnimated:NO]);
  OCMExpect([fake_reader_mode_chip_handler_ showReaderModeChip]);
  OCMExpect([contextual_panel_entrypoint_handler_
      cancelContextualPanelEntrypointLoudMoment]);
  GetWebStateList()->ActivateWebStateAt(1);
  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_chip_handler_);
  EXPECT_OCMOCK_VERIFY(contextual_panel_entrypoint_handler_);

  // Expect reader mode to be hidden without animation.
  OCMExpect([delegate_ readerModeBrowserAgent:GetReaderModeBrowserAgent()
                          hideContentAnimated:NO]);
  OCMExpect([fake_reader_mode_chip_handler_ hideReaderModeChip]);

  // Navigate to a new page.
  LoadWebpage(static_cast<web::FakeWebState*>(GetActiveWebState()), GURL());

  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(fake_reader_mode_chip_handler_);
}
