// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_dialog_overlay_utils.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/dialogs/ui_bundled/java_script_blocking_fake_web_state.h"
#import "ios/chrome/browser/dialogs/ui_bundled/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

using java_script_dialog_overlay::BlockDialogsButtonConfig;
using java_script_dialog_overlay::DialogMessage;
using java_script_dialog_overlay::DialogTitle;
using java_script_dialog_overlay::ShouldAddBlockDialogsButton;

typedef PlatformTest JavaScriptDialogOverlayUtilsTest;

// Tests that ShouldAddBlockDialogsButton returns the expected values.
TEST_F(JavaScriptDialogOverlayUtilsTest, ShouldAddBlockDialogsButton) {
  EXPECT_FALSE(ShouldAddBlockDialogsButton(/*web_state=*/nullptr));

  JavaScriptBlockingFakeWebState web_state;
  // Dialogs shouldn't be blocked if there is no JavaScriptDialogBlockingState
  // associated with `web_state`.
  ASSERT_FALSE(ShouldAddBlockDialogsButton(&web_state));

  JavaScriptDialogBlockingState::CreateForWebState(&web_state);
  // Blocking option if not shown if no dialog has been shown.
  ASSERT_FALSE(ShouldAddBlockDialogsButton(&web_state));
  JavaScriptDialogBlockingState::FromWebState(&web_state)
      ->JavaScriptDialogWasShown();
  ASSERT_TRUE(ShouldAddBlockDialogsButton(&web_state));
}

// Tests that BlockDialogsButtonConfig returns the expected button config.
TEST_F(JavaScriptDialogOverlayUtilsTest, BlockDialogsButtonConfig) {
  alert_overlays::ButtonConfig config = BlockDialogsButtonConfig();
  EXPECT_EQ(UIAlertActionStyleDestructive, config.style);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT),
      config.title);
}

// Tests that DialogTitle and DialogMessage return the expected values.
TEST_F(JavaScriptDialogOverlayUtilsTest, DialogTitleAndMessage) {
  NSString* dialog_message = @"message";

  EXPECT_NSEQ(dialog_message, DialogTitle(
                                  /*is_main_frame=*/true, dialog_message));
  EXPECT_NSEQ(nil, DialogMessage(
                       /*is_main_frame=*/true, dialog_message));

  NSString* iframe_title = l10n_util::GetNSString(
      IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
  EXPECT_NSEQ(iframe_title,
              DialogTitle(/*is_main_frame=*/false, dialog_message));
  EXPECT_NSEQ(dialog_message, DialogMessage(
                                  /*is_main_frame=*/false, dialog_message));
}
