// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_picker_handler.h"

#import <UIKit/UIKit.h>

#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/tab_picker/public/tab_picker_snackbar_presenter.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class GeminiTabPickerHandlerTest : public PlatformTest {
 protected:
  GeminiTabPickerHandlerTest() {
    mock_tab_picker_handler_ = OCMProtocolMock(@protocol(TabPickerCommands));
    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));

    handler_ = [[GeminiTabPickerHandler alloc] init];
    handler_.tabPickerHandler = mock_tab_picker_handler_;
    handler_.snackbarHandler = mock_snackbar_handler_;
  }

  GeminiTabPickerHandler* handler_;
  id mock_tab_picker_handler_;
  id mock_snackbar_handler_;
};

// Tests that the handler conforms to the GeminiTabPickerDelegate protocol.
TEST_F(GeminiTabPickerHandlerTest, TestConformsToProtocol) {
  EXPECT_TRUE([handler_ conformsToProtocol:@protocol(GeminiTabPickerDelegate)]);
}

// Tests that openTabPickerFromViewController correctly routes the request via
// command dispatcher.
TEST_F(GeminiTabPickerHandlerTest, TestOpenTabPicker) {
  UIViewController* mockViewController = OCMClassMock([UIViewController class]);

  OCMExpect([mock_tab_picker_handler_
      showTabPickerWithParams:[OCMArg checkWithBlock:^BOOL(
                                          TabPickerParams* params) {
        EXPECT_EQ(params.baseViewController, mockViewController);
        EXPECT_EQ(params.maxTabAttachmentCount, 10u);
        EXPECT_TRUE(params.preselectedWebStateIDs.empty());
        EXPECT_NE(params.snackbarPresenter, nil);
        return YES;
      }]
                   completion:[OCMArg any]]);

  [handler_ openTabPickerFromViewController:mockViewController];

  EXPECT_OCMOCK_VERIFY(mock_tab_picker_handler_);
}

// Tests that showing error snackbars records the corresponding user actions.
TEST_F(GeminiTabPickerHandlerTest, TestErrorSnackbarUserActions) {
  base::UserActionTester user_action_tester;
  id<TabPickerSnackbarPresenter> presenter =
      (id<TabPickerSnackbarPresenter>)handler_;

  [presenter showSnackbarForTabAttachmentLimit:10];
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "MobileGeminiTabPickerErrorAttachmentLimit"));

  [presenter showCannotReloadTabError];
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "MobileGeminiTabPickerErrorCannotReloadTab"));

  [presenter showCannotAttachTabError];
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "MobileGeminiTabPickerErrorCannotAttachTab"));
}
