// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_consumer.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

class SaveCardBottomSheetViewControllerTest : public PlatformTest {
 public:
  SaveCardBottomSheetViewControllerTest() {
    dataSource_ =
        OCMStrictProtocolMock(@protocol(SaveCardBottomSheetDataSource));
    mutator_ = OCMStrictProtocolMock(@protocol(SaveCardBottomSheetMutator));
    view_controller_ = [[SaveCardBottomSheetViewController alloc] init];
    view_controller_.dataSource = dataSource_;
  }

  ~SaveCardBottomSheetViewControllerTest() override {
    EXPECT_OCMOCK_VERIFY(dataSource_);
    EXPECT_OCMOCK_VERIFY(mutator_);
  }

 protected:
  void ViewSetup() {
    // Set dataSource_ properties when accessed on `viewDidLoad`.
    OCMStub([dataSource_ logoType]).andReturn(kGooglePayLogo);
    OCMStub([dataSource_ logoAccessibilityLabel]).andReturn(@"Google Pay");

    // Presence of primary action string lets view controller create a primary
    // action button.
    view_controller_.configuration.primaryActionString = @"Save";
    [view_controller_ reloadConfiguration];

    EXPECT_TRUE(view_controller_.view);
  }

  id<SaveCardBottomSheetDataSource> dataSource_;
  id<SaveCardBottomSheetMutator> mutator_;
  SaveCardBottomSheetViewController* view_controller_;
};

// TODO(crbug.com/422437418): re-enable.
TEST_F(SaveCardBottomSheetViewControllerTest, DISABLED_BottomSheetAccepted) {
  OCMExpect([mutator_ didAccept]);
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
}

// TODO(crbug.com/422437418): re-enable.
TEST_F(SaveCardBottomSheetViewControllerTest, DISABLED_BottomSheetCancelled) {
  OCMExpect([mutator_ didCancel]);
  [view_controller_.actionHandler confirmationAlertSecondaryAction];
}

TEST_F(SaveCardBottomSheetViewControllerTest, ShowLoading) {
  ViewSetup();
  [view_controller_ showLoadingStateWithAccessibilityLabel:@"A11y label"];
  EXPECT_FALSE(view_controller_.primaryActionButton.enabled);
  EXPECT_NSEQ(view_controller_.primaryActionButton.accessibilityLabel,
              @"A11y label");
}

TEST_F(SaveCardBottomSheetViewControllerTest, ShowConfirmation) {
  ViewSetup();
  [view_controller_ showConfirmationState];
  EXPECT_FALSE(view_controller_.primaryActionButton.enabled);
  EXPECT_NSEQ(view_controller_.primaryActionButton.accessibilityLabel,
              l10n_util::GetNSString(
                  IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME));
}
