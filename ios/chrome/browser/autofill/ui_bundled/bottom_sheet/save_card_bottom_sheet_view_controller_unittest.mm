// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_view_controller.h"

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

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
