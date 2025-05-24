// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

class SaveCardBottomSheetViewControllerTest : public PlatformTest {
 public:
  SaveCardBottomSheetViewControllerTest() {
    mutator_ = OCMStrictProtocolMock(@protocol(SaveCardBottomSheetMutator));
    view_controller_ = [[SaveCardBottomSheetViewController alloc] init];
  }

 protected:
  id<SaveCardBottomSheetMutator> mutator_;
  SaveCardBottomSheetViewController* view_controller_;
};

TEST_F(SaveCardBottomSheetViewControllerTest, BottomSheetAccepted) {
  OCMExpect([mutator_ didAccept]);
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
}

TEST_F(SaveCardBottomSheetViewControllerTest, BottomSheetCancelled) {
  OCMExpect([mutator_ didCancel]);
  [view_controller_.actionHandler confirmationAlertSecondaryAction];
}

TEST_F(SaveCardBottomSheetViewControllerTest, ShowLoading) {
  // Presence of primary action string lets view controller create a primary
  // action button.
  view_controller_.primaryActionString = @"Save";
  EXPECT_TRUE(view_controller_.view);
  [view_controller_ showLoadingStateWithAccessibilityLabel:@"A11y label"];
  EXPECT_TRUE(view_controller_.isLoading);
  EXPECT_FALSE(view_controller_.isConfirmed);
  EXPECT_NSEQ(view_controller_.primaryActionButton.accessibilityLabel,
              @"A11y label");
}

TEST_F(SaveCardBottomSheetViewControllerTest, ShowConfirmation) {
  // Presence of primary action string lets view controller create a primary
  // action button.
  view_controller_.primaryActionString = @"Save";

  EXPECT_TRUE(view_controller_.view);
  [view_controller_ showConfirmationState];
  EXPECT_FALSE(view_controller_.isLoading);
  EXPECT_TRUE(view_controller_.isConfirmed);
  EXPECT_NSEQ(view_controller_.primaryActionButton.accessibilityLabel,
              l10n_util::GetNSString(
                  IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME));
}
