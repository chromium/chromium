// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_view_controller.h"

#import <string>
#import <string_view>

#import "base/strings/string_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/time/time.h"
#import "components/plus_addresses/features.h"
#import "components/plus_addresses/metrics/plus_address_metrics.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_delegate.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

using PlusAddressModalCompletionStatus =
    plus_addresses::metrics::PlusAddressModalCompletionStatus;
using PlusAddressModalEvent = plus_addresses::metrics::PlusAddressModalEvent;

constexpr std::string_view kPlusAddressModalEventHistogram =
    "PlusAddresses.Modal.Events";
constexpr std::string_view kPlusAddressModalWithNoticeEventHistogram =
    "PlusAddresses.ModalWithNotice.Events";
constexpr base::TimeDelta kDuration = base::Milliseconds(3600);

std::string FormatModalDurationMetrics(PlusAddressModalCompletionStatus status,
                                       bool notice_shown) {
  return base::ReplaceStringPlaceholders(
      notice_shown ? "PlusAddresses.ModalWithNotice.$1.ShownDuration"
                   : "PlusAddresses.Modal.$1.ShownDuration",
      {plus_addresses::metrics::PlusAddressModalCompletionStatusToString(
          status)},
      /*offsets=*/nullptr);
}

std::string FormatRefreshHistogramNameFor(
    PlusAddressModalCompletionStatus status,
    bool notice_shown) {
  return base::ReplaceStringPlaceholders(
      notice_shown ? "PlusAddresses.ModalWithNotice.$1.Refreshes"
                   : "PlusAddresses.Modal.$1.Refreshes",
      {plus_addresses::metrics::PlusAddressModalCompletionStatusToString(
          status)},
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
      BucketsAre(base::Bucket(PlusAddressModalEvent::kModalShown, 1),
                 base::Bucket(PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/false),
      /*refresh_count=*/0, 1);
}

// Ensure that tapping the confirm button onthe bottom sheet confirms the
// plus_address and collects notice-specific metrics.
TEST_F(PlusAddressBottomSheetViewControllerTest,
       ConfirmButtonTappedWithNoticeShown) {
  base::test::ScopedFeatureList feature_list{
      plus_addresses::features::kPlusAddressUserOnboardingEnabled};
  OCMStub([delegate_ shouldShowNotice]).andReturn(YES);

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
      histogram_tester_.GetAllSamples(
          kPlusAddressModalWithNoticeEventHistogram),
      BucketsAre(base::Bucket(PlusAddressModalEvent::kModalShown, 1),
                 base::Bucket(PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/true),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/true),
      /*refresh_count=*/0, 1);
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
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(base::Bucket(PlusAddressModalEvent::kModalShown, 1),
                 base::Bucket(PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressModalCompletionStatus::kModalCanceled,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          PlusAddressModalCompletionStatus::kModalCanceled,
          /*notice_shown=*/false),
      /*refresh_count=*/0, 1);
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
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(base::Bucket(PlusAddressModalEvent::kModalShown, 1),
                 base::Bucket(PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressModalCompletionStatus::kModalCanceled,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          PlusAddressModalCompletionStatus::kModalCanceled,
          /*notice_shown=*/false),
      /*refresh_count=*/0, 1);
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
  [view_controller_
      notifyError:PlusAddressModalCompletionStatus::kConfirmPlusAddressError];

  // Tap cancel button.
  [view_controller_.actionHandler confirmationAlertSecondaryAction];

  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(base::Bucket(PlusAddressModalEvent::kModalShown, 1),
                 base::Bucket(PlusAddressModalEvent::kModalConfirmed, 1),
                 base::Bucket(PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressModalCompletionStatus::kConfirmPlusAddressError,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          PlusAddressModalCompletionStatus::kConfirmPlusAddressError,
          /*notice_shown=*/false),
      /*refresh_count=*/0, 1);
}

// Ensure that when reservation error occurs, user can tap cancel button to
// dismiss the bottom sheet and metric for the reservation error is collected.
TEST_F(PlusAddressBottomSheetViewControllerTest, CancelAfterReserveError) {
  OCMExpect([delegate_ reservePlusAddress]);
  OCMExpect([browser_coordinator_commands_ dismissPlusAddressBottomSheet]);

  [view_controller_ loadViewIfNeeded];

  scoped_clock_.Advance(kDuration);

  // Simulate error occurring during plus address reservation.
  [view_controller_
      notifyError:PlusAddressModalCompletionStatus::kReservePlusAddressError];

  // Tap cancel button.
  [view_controller_.actionHandler confirmationAlertSecondaryAction];

  EXPECT_OCMOCK_VERIFY(browser_coordinator_commands_);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(base::Bucket(PlusAddressModalEvent::kModalShown, 1),
                 base::Bucket(PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressModalCompletionStatus::kReservePlusAddressError,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          PlusAddressModalCompletionStatus::kReservePlusAddressError,
          /*notice_shown=*/false),
      /*refresh_count=*/0, 1);
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
  [view_controller_
      notifyError:PlusAddressModalCompletionStatus::kConfirmPlusAddressError];

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
      BucketsAre(base::Bucket(PlusAddressModalEvent::kModalShown, 1),
                 base::Bucket(PlusAddressModalEvent::kModalConfirmed, 1),
                 base::Bucket(PlusAddressModalEvent::kModalCanceled, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressModalCompletionStatus::kConfirmPlusAddressError,
          /*notice_shown=*/false),
      kDuration, 1);
  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          PlusAddressModalCompletionStatus::kConfirmPlusAddressError,
          /*notice_shown=*/false),
      /*refresh_count=*/0, 1);
}

// Ensure that tapping on the refresh button and then confirming the plusAddress
// logs appopriate metrics.
TEST_F(PlusAddressBottomSheetViewControllerTest,
       RefreshAndConfirmButtonTapped) {
  OCMExpect([delegate_ reservePlusAddress]);
  [view_controller_ loadViewIfNeeded];

  OCMExpect([delegate_ confirmPlusAddress]);
  OCMExpect([delegate_ didTapRefreshButton]);

  [view_controller_ didTapTrailingButton];

  scoped_clock_.Advance(kDuration);

  // Test function called after user taps confirm button.
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
  EXPECT_OCMOCK_VERIFY(delegate_);

  // Simulate mediator notifying controller of successful plus address
  // confirmation.
  [view_controller_ didConfirmPlusAddress];

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kPlusAddressModalEventHistogram),
      BucketsAre(base::Bucket(PlusAddressModalEvent::kModalShown, 1),
                 base::Bucket(PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester_.ExpectUniqueTimeSample(
      FormatModalDurationMetrics(
          PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/false),
      kDuration, 1);

  histogram_tester_.ExpectUniqueSample(
      FormatRefreshHistogramNameFor(
          PlusAddressModalCompletionStatus::kModalConfirmed,
          /*notice_shown=*/false),
      /*refresh_count=*/1, 1);
}
