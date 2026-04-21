// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_view_controller.h"

#import <string>
#import <string_view>

#import "base/strings/string_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/time/time.h"
#import "components/plus_addresses/core/common/features.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_delegate.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

constexpr base::TimeDelta kDuration = base::Milliseconds(3600);

}  // namespace

class PlusAddressBottomSheetViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    delegate_ = OCMProtocolMock(@protocol(PlusAddressBottomSheetDelegate));

    browser_coordinator_commands_ =
        OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
    view_controller_ = [[PlusAddressBottomSheetViewController alloc]
                      initWithDelegate:delegate_
        withBrowserCoordinatorCommands:browser_coordinator_commands_];
  }

 protected:
  id delegate_;
  id browser_coordinator_commands_;
  PlusAddressBottomSheetViewController* view_controller_;
  base::HistogramTester histogram_tester_;
  base::Time start_time_ = base::Time::FromSecondsSinceUnixEpoch(1);
  base::ScopedMockClockOverride scoped_clock_;
};

// Ensure that tapping confirm button on bottom sheet confirms plus_address.
TEST_F(PlusAddressBottomSheetViewControllerTest, ConfirmButtonTapped) {
  base::UserActionTester user_action_tester;
  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);

  scoped_clock_.Advance(kDuration);

  // Test function called after user taps confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  // Simulate mediator notifying controller of successful plus address
  // confirmation.
  [view_controller_ didConfirmPlusAddress];

  EXPECT_EQ(user_action_tester.GetActionCount(
                "PlusAddresses.OfferedPlusAddressAccepted"),
            1);
}

// Ensure that tapping the confirm button onthe bottom sheet confirms the
// plus_address.
TEST_F(PlusAddressBottomSheetViewControllerTest,
       ConfirmButtonTappedWithNoticeShown) {
  OCMStub([delegate_ shouldShowNotice]).andReturn(YES);

  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);

  scoped_clock_.Advance(kDuration);

  // Test function called after user taps confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  // Simulate mediator notifying controller of successful plus address
  // confirmation.
  [view_controller_ didConfirmPlusAddress];
}

// Ensure that tapping cancel button dismisses bottom sheet.
TEST_F(PlusAddressBottomSheetViewControllerTest, CancelButtonTapped) {
  base::UserActionTester user_action_tester;
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);
  [view_controller_ loadViewIfNeeded];

  scoped_clock_.Advance(kDuration);

  // Cancel plus_address is implemented with secondary button.
  EXPECT_TRUE([view_controller_.actionHandler
      respondsToSelector:@selector(confirmationAlertSecondaryAction)]);
  // Test function called after user taps cancel button.
  [view_controller_.actionHandler confirmationAlertSecondaryAction];

  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "PlusAddresses.OfferedPlusAddressDeclined"),
            1);
}

// Simulate a swipe to dismisses bottom sheet.
TEST_F(PlusAddressBottomSheetViewControllerTest, SwipeToDismiss) {
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);
  [view_controller_ loadViewIfNeeded];

  scoped_clock_.Advance(kDuration);

  // Test function called after user swipe to dismiss bottom sheet.
  UIPresentationController* presentationController =
      view_controller_.presentationController;
  EXPECT_TRUE([presentationController.delegate
      respondsToSelector:@selector(presentationControllerDidDismiss:)]);
  [presentationController.delegate
      presentationControllerDidDismiss:presentationController];
  EXPECT_OCMOCK_VERIFY(delegate_);
  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
}

// Ensure that when confirmation error occurs, user can tap cancel button to
// dismiss the bottom sheet.
TEST_F(PlusAddressBottomSheetViewControllerTest, CancelAfterConfirmError) {
  base::UserActionTester user_action_tester;
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);

  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);

  // Tap confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  scoped_clock_.Advance(kDuration);

  // Simulate error occurring during plus address confirmation.
  [view_controller_
      notifyError:plus_addresses::PlusAddressCreationBottomSheetErrorType::
                      kCreateGeneric];

  // Tap cancel button.
  [view_controller_.actionHandler confirmationAlertSecondaryAction];

  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
}

// Ensure that when reservation error occurs, user can tap cancel button to
// dismiss the bottom sheet.
TEST_F(PlusAddressBottomSheetViewControllerTest, CancelAfterReserveError) {
  base::UserActionTester user_action_tester;
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);

  [view_controller_ loadViewIfNeeded];

  scoped_clock_.Advance(kDuration);

  // Simulate error occurring during plus address reservation.
  [view_controller_
      notifyError:plus_addresses::PlusAddressCreationBottomSheetErrorType::
                      kNoError];

  // Tap cancel button.
  [view_controller_.actionHandler confirmationAlertSecondaryAction];

  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
}

// Ensure that when confirmation error occurs, user swipe to dismiss the bottom
// sheet.
TEST_F(PlusAddressBottomSheetViewControllerTest, DismissAfterConfirmError) {
  base::UserActionTester user_action_tester;
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);

  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);

  // Tap confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  scoped_clock_.Advance(kDuration);

  // Simulate error occurring during plus address confirmation.
  [view_controller_
      notifyError:plus_addresses::PlusAddressCreationBottomSheetErrorType::
                      kCreateGeneric];

  // Dismiss bottom sheet.
  UIPresentationController* presentationController =
      view_controller_.presentationController;
  EXPECT_TRUE([presentationController.delegate
      respondsToSelector:@selector(presentationControllerDidDismiss:)]);
  [presentationController.delegate
      presentationControllerDidDismiss:presentationController];

  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
}

// Ensure that when confirmation error occurs, user swipe to dismiss the bottom
// sheet and metric for the affiliation cancelation is collected.
TEST_F(PlusAddressBottomSheetViewControllerTest, CancelAfterAffiliationError) {
  base::UserActionTester user_action_tester;
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);

  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);

  // Tap confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  scoped_clock_.Advance(kDuration);

  // Simulate error occurring during plus address confirmation.
  [view_controller_
      notifyError:plus_addresses::PlusAddressCreationBottomSheetErrorType::
                      kCreateAffiliation];

  // Dismiss bottom sheet.
  UIPresentationController* presentationController =
      view_controller_.presentationController;
  EXPECT_TRUE([presentationController.delegate
      respondsToSelector:@selector(presentationControllerDidDismiss:)]);
  [presentationController.delegate
      presentationControllerDidDismiss:presentationController];

  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "PlusAddresses.AffiliationErrorCanceled"),
            1);
}

// Ensure that when confirmation error occurs, user accepts the bottom
// sheet and metric for the quota limit is collected.
TEST_F(PlusAddressBottomSheetViewControllerTest, CancelAfterQuotaError) {
  base::UserActionTester user_action_tester;
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);

  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);

  // Tap confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  scoped_clock_.Advance(kDuration);

  // Simulate error occurring during plus address confirmation.
  [view_controller_
      notifyError:plus_addresses::PlusAddressCreationBottomSheetErrorType::
                      kCreateQuota];

  // Dismiss bottom sheet.
  UIPresentationController* presentationController =
      view_controller_.presentationController;
  EXPECT_TRUE([presentationController.delegate
      respondsToSelector:@selector(presentationControllerDidDismiss:)]);
  [presentationController.delegate
      presentationControllerDidDismiss:presentationController];

  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
  EXPECT_EQ(
      user_action_tester.GetActionCount("PlusAddresses.QuotaErrorAccepted"), 1);
}

// Ensure that tapping on the refresh button and then confirming the
// plusAddress.
TEST_F(PlusAddressBottomSheetViewControllerTest,
       RefreshAndConfirmButtonTapped) {
  base::UserActionTester user_action_tester;
  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);
  OCMExpect([delegate_ didTapRefreshButton]);

  [view_controller_ didTapTrailingButton];
  EXPECT_EQ(user_action_tester.GetActionCount("PlusAddresses.Refreshed"), 1);
  scoped_clock_.Advance(kDuration);

  // Test function called after user taps confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  // Simulate mediator notifying controller of successful plus address
  // confirmation.
  [view_controller_ didConfirmPlusAddress];
}
