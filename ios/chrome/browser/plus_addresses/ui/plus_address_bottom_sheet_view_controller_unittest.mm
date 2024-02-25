// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_view_controller.h"

#import "base/strings/string_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/time/time.h"
#import "components/plus_addresses/plus_address_metrics.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_delegate.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const char kPlusAddressModalEventHistogram[] =
    "Autofill.PlusAddresses.Modal.Events";
const base::TimeDelta kDuration = base::Milliseconds(3600);
std::string FormatModalDurationMetrics(
    plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus
        status) {
  return base::ReplaceStringPlaceholders(
      "Autofill.PlusAddresses.Modal.$1.ShownDuration",
      {plus_addresses::PlusAddressMetrics::
           PlusAddressModalCompletionStatusToString(status)},
      /*offsets=*/nullptr);
}

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

// Ensure that tapping confirm button on bottom sheet confirms plus_address
// and collects relevant metrics.
TEST_F(PlusAddressBottomSheetViewControllerTest, ConfirmButtonTapped) {
  OCMExpect([delegate_ reservePlusAddress]);
  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);

  scoped_clock_.Advance(kDuration);

  // Test function called after user taps confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  // Simulate mediator notifying controller of successful plus address
  // confirmation.
  [view_controller_ didConfirmPlusAddress];

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(base::Bucket(plus_addresses::PlusAddressMetrics::
                                  PlusAddressModalEvent::kModalShown,
                              1),
                 base::Bucket(plus_addresses::PlusAddressMetrics::
                                  PlusAddressModalEvent::kModalConfirmed,
                              1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus::
              kModalConfirmed),
      kDuration, 1);
}

// Ensure that tapping cancel button dismisses bottom sheet
// and collects relevant metrics.
TEST_F(PlusAddressBottomSheetViewControllerTest, CancelButtonTapped) {
  OCMExpect([delegate_ reservePlusAddress]);
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
  EXPECT_THAT(histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
              BucketsAre(base::Bucket(plus_addresses::PlusAddressMetrics::
                                          PlusAddressModalEvent::kModalShown,
                                      1),
                         base::Bucket(plus_addresses::PlusAddressMetrics::
                                          PlusAddressModalEvent::kModalCanceled,
                                      1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus::
              kModalCanceled),
      kDuration, 1);
}

// Simulate a swipe to dismisses bottom sheet and ensure that
// relevant metrics are collected.
TEST_F(PlusAddressBottomSheetViewControllerTest, SwipeToDismiss) {
  OCMExpect([delegate_ reservePlusAddress]);
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
  EXPECT_THAT(histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
              BucketsAre(base::Bucket(plus_addresses::PlusAddressMetrics::
                                          PlusAddressModalEvent::kModalShown,
                                      1),
                         base::Bucket(plus_addresses::PlusAddressMetrics::
                                          PlusAddressModalEvent::kModalCanceled,
                                      1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus::
              kModalCanceled),
      kDuration, 1);
}

// Ensure that when confirmation error occurs, user can tap cancel button to
// dismiss the bottom sheet and metric for the confirmation error is collected.
TEST_F(PlusAddressBottomSheetViewControllerTest, CancelAfterConfirmError) {
  OCMExpect([delegate_ reservePlusAddress]);
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);

  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);

  // Tap confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  scoped_clock_.Advance(kDuration);

  // Simulate error occurring during plus address confirmation.
  [view_controller_ notifyError:plus_addresses::PlusAddressMetrics::
                                    PlusAddressModalCompletionStatus::
                                        kConfirmPlusAddressError];

  // Tap cancel button.
  [view_controller_.actionHandler confirmationAlertSecondaryAction];

  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(base::Bucket(plus_addresses::PlusAddressMetrics::
                                  PlusAddressModalEvent::kModalShown,
                              1),
                 base::Bucket(plus_addresses::PlusAddressMetrics::
                                  PlusAddressModalEvent::kModalConfirmed,
                              1),
                 base::Bucket(plus_addresses::PlusAddressMetrics::
                                  PlusAddressModalEvent::kModalCanceled,
                              1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus::
              kConfirmPlusAddressError),
      kDuration, 1);
}

// Ensure that when reservation error occurs, user can tap cancel button to
// dismiss the bottom sheet and metric for the reservation error is collected.
TEST_F(PlusAddressBottomSheetViewControllerTest, CancelAfterReserveError) {
  OCMExpect([delegate_ reservePlusAddress]);
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);

  [view_controller_ loadViewIfNeeded];

  scoped_clock_.Advance(kDuration);

  // Simulate error occurring during plus address reservation.
  [view_controller_ notifyError:plus_addresses::PlusAddressMetrics::
                                    PlusAddressModalCompletionStatus::
                                        kReservePlusAddressError];

  // Tap cancel button.
  [view_controller_.actionHandler confirmationAlertSecondaryAction];

  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
  EXPECT_THAT(histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
              BucketsAre(base::Bucket(plus_addresses::PlusAddressMetrics::
                                          PlusAddressModalEvent::kModalShown,
                                      1),
                         base::Bucket(plus_addresses::PlusAddressMetrics::
                                          PlusAddressModalEvent::kModalCanceled,
                                      1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus::
              kReservePlusAddressError),
      kDuration, 1);
}

// Ensure that when confirmation error occurs, user swipe to dismiss the bottom
// sheet and metric for the confirmation error is collected.
TEST_F(PlusAddressBottomSheetViewControllerTest, DismissAfterConfirmError) {
  OCMExpect([delegate_ reservePlusAddress]);
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);

  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);

  // Tap confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  scoped_clock_.Advance(kDuration);

  // Simulate error occurring during plus address confirmation.
  [view_controller_ notifyError:plus_addresses::PlusAddressMetrics::
                                    PlusAddressModalCompletionStatus::
                                        kConfirmPlusAddressError];

  // Dismiss bottom sheet.
  UIPresentationController* presentationController =
      view_controller_.presentationController;
  EXPECT_TRUE([presentationController.delegate
      respondsToSelector:@selector(presentationControllerDidDismiss:)]);
  [presentationController.delegate
      presentationControllerDidDismiss:presentationController];

  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(base::Bucket(plus_addresses::PlusAddressMetrics::
                                  PlusAddressModalEvent::kModalShown,
                              1),
                 base::Bucket(plus_addresses::PlusAddressMetrics::
                                  PlusAddressModalEvent::kModalConfirmed,
                              1),
                 base::Bucket(plus_addresses::PlusAddressMetrics::
                                  PlusAddressModalEvent::kModalCanceled,
                              1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus::
              kConfirmPlusAddressError),
      kDuration, 1);
}
