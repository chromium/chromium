// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/scan_decision_helper.h"

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/enterprise_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/fakes/fake_enterprise_commands_handler.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace enterprise_connectors {

// Unit tests for ScanDecisionHandler testing that base on the file scanning
// result, 1. the correct warning dialog or snackbar will be shown; and 2.
// whether the file download will be blocked.
class ScanDecisionHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestProfileIOS::Builder profile_builder;
    profile_ = std::move(profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    fake_commands_handler_ = [[FakeEnterpriseCommandsHandler alloc] init];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:fake_commands_handler_
                     forProtocol:@protocol(EnterpriseCommands)];
    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];

    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    web_state->SetBrowserState(profile_.get());

    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());

    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));
    web_state_->WasShown();
  }

  void TearDown() override {
    web_state_ = nullptr;
    profile_ = nullptr;
    mock_snackbar_handler_ = nullptr;
    fake_commands_handler_ = nullptr;
    browser_.reset();
  }

  // Creates a RequestHandlerResult with the FinalContentAnalysisRsult, which
  // holds the file scanning result.
  RequestHandlerResult CreateResult(FinalContentAnalysisResult final_result) {
    RequestHandlerResult result;
    result.final_result = final_result;
    return result;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<web::FakeWebState> web_state_;
  FakeEnterpriseCommandsHandler* fake_commands_handler_;
  id mock_snackbar_handler_;
};

// Tests that file download can proceed if scan result is success.
TEST_F(ScanDecisionHelperTest, ScanResultSuccess) {
  base::test::TestFuture<bool> future;

  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::SUCCESS);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  EXPECT_TRUE(future.Get());
}

// Tests that file download can proceed if scan result is warning and user
// choose to ignore.
TEST_F(ScanDecisionHelperTest, ScanResultWarnProcceed) {
  base::test::TestFuture<bool> future;

  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::WARNING);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  std::move(fake_commands_handler_->_callback).Run(true);
  EXPECT_TRUE(future.Get());
}

// Tests that file download is blocked if scan result is warning and user
// choose to cancel.
TEST_F(ScanDecisionHelperTest, ScanResultWarnCancel) {
  base::test::TestFuture<bool> future;

  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::WARNING);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  std::move(fake_commands_handler_->_callback).Run(false);
  EXPECT_FALSE(future.Get());
}

// Tests that file download is blocked if scan result is file too large.
TEST_F(ScanDecisionHelperTest, ScanResultLargeFiles) {
  base::test::TestFuture<bool> future;

  OCMExpect([mock_snackbar_handler_
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg any]]);
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::LARGE_FILES);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  EXPECT_FALSE(future.Get());
  [mock_snackbar_handler_ verify];
}

// Tests that file download is blocked if scan result is failure.
TEST_F(ScanDecisionHelperTest, ScanResultFailure) {
  base::test::TestFuture<bool> future;

  OCMExpect([mock_snackbar_handler_
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg any]]);
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::FAILURE);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  EXPECT_FALSE(future.Get());
  [mock_snackbar_handler_ verify];
}

// Tests that file download is blocked if scan result is file closed.
TEST_F(ScanDecisionHelperTest, ScanResultClosed) {
  base::test::TestFuture<bool> future;

  OCMExpect([mock_snackbar_handler_
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg any]]);
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::FAIL_CLOSED);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  EXPECT_FALSE(future.Get());
  [mock_snackbar_handler_ verify];
}

// Tests that file download is blocked if web_state is null.
TEST_F(ScanDecisionHelperTest, NullWebState) {
  base::test::TestFuture<bool> future;

  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::SUCCESS);
  HandleScanDecision(nullptr, TriggerType::kSavePrompt, future.GetCallback(),
                     result);

  EXPECT_FALSE(future.Get());
}

// Tests that the scan decision UI is deferred when the WebState is not active.
TEST_F(ScanDecisionHelperTest, DeferredNotification) {
  base::test::TestFuture<bool> future;

  // Insert a second WebState and activate it.
  std::unique_ptr<web::FakeWebState> web_state2 =
      std::make_unique<web::FakeWebState>();
  web_state2->SetBrowserState(profile_.get());
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state2),
      WebStateList::InsertionParams::Automatic().Activate(true));
  web_state_->WasHidden();
  ASSERT_FALSE(web_state_->IsVisible());

  // Trigger a warning scan decision for the inactive web_state_.
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::WARNING);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  // Verify that the dialog has NOT been shown yet.
  EXPECT_TRUE(fake_commands_handler_->_callback.is_null());
  web_state_->WasShown();
  EXPECT_FALSE(fake_commands_handler_->_callback.is_null());
  std::move(fake_commands_handler_->_callback).Run(true);
  EXPECT_TRUE(future.Get());
}

// Tests that the snackbar notification is deferred when the WebState is not
// active.
TEST_F(ScanDecisionHelperTest, DeferredSnackbarNotification) {
  base::test::TestFuture<bool> future;
  web_state_->WasHidden();
  ASSERT_FALSE(web_state_->IsVisible());

  // Trigger a failure scan decision for the inactive web_state_.
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::FAILURE);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  // Verify that the download is blocked immediately even if UI is deferred.
  EXPECT_FALSE(future.Get());
  OCMExpect([mock_snackbar_handler_
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg any]]);
  web_state_->WasShown();
  [mock_snackbar_handler_ verify];
}

// Tests that closing the tab while a decision is pending correctly blocks the
// download and cleans up.
TEST_F(ScanDecisionHelperTest, CloseTabWithPendingDecision) {
  base::test::TestFuture<bool> future;
  web_state_->WasHidden();

  // Trigger a warning scan decision for the inactive web_state_.
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::WARNING);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  // Verify that the dialog has NOT been shown.
  EXPECT_TRUE(fake_commands_handler_->_callback.is_null());
  web_state_ = nullptr;
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kUserAction);
  EXPECT_FALSE(future.Get());
}

}  // namespace enterprise_connectors
