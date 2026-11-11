// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_mutator.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Tests for `RemoteActorCredentialBottomSheetViewController`.
class RemoteActorCredentialBottomSheetViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    mutator_ = OCMStrictProtocolMock(
        @protocol(RemoteActorCredentialBottomSheetMutator));
    view_controller_ =
        [[RemoteActorCredentialBottomSheetViewController alloc] init];
    view_controller_.mutator = mutator_;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mutator_);
    PlatformTest::TearDown();
  }

  id mutator_;
  RemoteActorCredentialBottomSheetViewController* view_controller_;
};

// Tests that the buttons display the allow and cancel titles.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest, TestButtonTitles) {
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_REMOTE_ACTOR_SHARING_DIALOG_ALLOW_BUTTON),
      view_controller_.configuration.primaryActionString);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL),
              view_controller_.configuration.secondaryActionString);
}

// Tests that the secondary action notifies the mutator of the cancellation.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TestSecondaryActionCancels) {
  OCMExpect([mutator_ didCancel]);
  [view_controller_.actionHandler confirmationAlertSecondaryAction];
}

// Tests that dismissing the bottom sheet notifies the mutator of the
// cancellation.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TestDismissalCancels) {
  OCMExpect([mutator_ didCancel]);
  [view_controller_.actionHandler confirmationAlertDismissed];
}
