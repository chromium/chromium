// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_view.h"

#import <optional>

#import "base/time/time.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_view+testing.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {
ParcelTrackingItem* createParcelTrackingItem(
    ParcelState status,
    std::optional<base::Time> estimated_delivery) {
  ParcelTrackingItem* item = [[ParcelTrackingItem alloc] init];
  item.parcelType = ParcelType::kUSPS;
  item.estimatedDeliveryTime = estimated_delivery;
  item.parcelID = @"parcel_id";
  item.trackingURL = GURL("https://usps.example/");
  item.status = status;
  item.commandHandler = nil;

  return item;
}

std::string parcelStateToString(ParcelState status) {
  switch (status) {
    case ParcelState::kUnkown:
      return "kUnkown";
    case ParcelState::kNew:
      return "kNew";
    case ParcelState::kLabelCreated:
      return "kLabelCreated";
    case ParcelState::kPickedUp:
      return "kPickedUp";
    case ParcelState::kFinished:
      return "kFinished";
    case ParcelState::kDeliveryFailed:
      return "kDeliveryFailed";
    case ParcelState::kError:
      return "kError";
    case ParcelState::kCancelled:
      return "kCancelled";
    case ParcelState::kOrderTooOld:
      return "kOrderTooOld";
    case ParcelState::kHandedOff:
      return "kHandedOff";
    case ParcelState::kWithCarrier:
      return "kWithCarrier";
    case ParcelState::kOutForDelivery:
      return "kOutForDelivery";
    case ParcelState::kAtPickupLocation:
      return "kAtPickupLocation";
    case ParcelState::kReturnToSender:
      return "kReturnToSender";
    case ParcelState::kReturnCompleted:
      return "kReturnCompleted";
    case ParcelState::kUndeliverable:
      return "kUndeliverable";
  }
}
}  // namespace

using ParcelTrackingViewTest = PlatformTest;

// Test that in-progress parcels have the correct title string when there is an
// estimated delivery date present.
TEST_F(ParcelTrackingViewTest, InProgressParcelsWithEstimatedDeliveryDate) {
  const ParcelState in_progress_states[] = {ParcelState::kPickedUp,
                                            ParcelState::kHandedOff,
                                            ParcelState::kWithCarrier};
  NSString* new_status_string = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_NEW_STATUS);

  base::Time estimated_delivery_date = base::Time::Now() + base::Days(5);
  for (const auto& state : in_progress_states) {
    ParcelTrackingItem* item =
        createParcelTrackingItem(state, estimated_delivery_date);
    ParcelTrackingModuleView* view =
        [[ParcelTrackingModuleView alloc] initWithFrame:CGRectZero];
    [view configureView:item];
    // To avoid over-fitting the test, we only check that the title string is
    // not the 'new parcel' one.
    EXPECT_NSNE(view.titleLabelTextForTesting, new_status_string)
        << "(state = " << parcelStateToString(state) << ")";
  }
}

// Test that in-progress parcels have the correct title string when there is not
// estimated delivery date from the server.
TEST_F(ParcelTrackingViewTest, InProgressParcelsWithoutEstimatedDeliveryDate) {
  const ParcelState in_progress_states[] = {ParcelState::kPickedUp,
                                            ParcelState::kHandedOff,
                                            ParcelState::kWithCarrier};
  NSString* new_status_string = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_NEW_STATUS);

  for (const auto& state : in_progress_states) {
    ParcelTrackingItem* item =
        createParcelTrackingItem(state, /*estimated_delivery=*/std::nullopt);
    ParcelTrackingModuleView* view =
        [[ParcelTrackingModuleView alloc] initWithFrame:CGRectZero];
    [view configureView:item];
    // When there is no estimated delivery date, we display the same title as-if
    // the parcel was in the 'new' status.
    EXPECT_NSEQ(view.titleLabelTextForTesting, new_status_string)
        << "(state = " << parcelStateToString(state) << ")";
  }
}

// Test that delivered parcels have the correct title string based on their
// delivery date.
TEST_F(ParcelTrackingViewTest, FinishedParcelsWithEstimatedDeliveryDate) {
  NSString* delivered_string = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_DELIVERED_STATUS);

  // Packages that were delivered today have their own string.
  NSString* delivered_today_string = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_DELIVERED_TODAY);
  ParcelTrackingItem* item = createParcelTrackingItem(
      ParcelState::kFinished, /*estimated_delivery=*/base::Time::Now());
  ParcelTrackingModuleView* view =
      [[ParcelTrackingModuleView alloc] initWithFrame:CGRectZero];
  [view configureView:item];
  EXPECT_TRUE([view.titleLabelTextForTesting containsString:delivered_string]);
  EXPECT_TRUE(
      [view.titleLabelTextForTesting containsString:delivered_today_string]);

  // Otherwise the string will depend on the estimated delivery date. With an
  // estimated delivery date present, the date is included in the title.
  base::Time estimated_delivered_date = base::Time::Now() - base::Days(2);
  item = createParcelTrackingItem(ParcelState::kFinished,
                                  estimated_delivered_date);
  view = [[ParcelTrackingModuleView alloc] initWithFrame:CGRectZero];
  [view configureView:item];
  EXPECT_TRUE([view.titleLabelTextForTesting containsString:delivered_string]);
  // The string should also include the estimated delivery date, so it shouldn't
  // be exactly the static string.
  EXPECT_NSNE(view.titleLabelTextForTesting, delivered_string);
}

// Test that delivered parcels have the correct title string when there is no
// estimated delivery date from the server.
TEST_F(ParcelTrackingViewTest, FinishedParcelsWithoutEstimatedDeliveryDate) {
  NSString* delivered_string = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_DELIVERED_STATUS);

  // When there is no estimated delivery date, the date is excluded from the
  // title and it should be exactly `delivered_string`.
  ParcelTrackingItem* item =
      createParcelTrackingItem(ParcelState::kFinished,
                               /*estimated_delivery=*/std::nullopt);
  ParcelTrackingModuleView* view =
      [[ParcelTrackingModuleView alloc] initWithFrame:CGRectZero];
  [view configureView:item];
  EXPECT_NSEQ(view.titleLabelTextForTesting, delivered_string);
}

// Tests that parcels in states other than the above do not have their views
// affected by the estimated delivery date.
TEST_F(ParcelTrackingViewTest, OtherParcelStatesEstimatedDeliveryState) {
  const ParcelState states[] = {
      ParcelState::kUnkown,         ParcelState::kNew,
      ParcelState::kLabelCreated,   ParcelState::kAtPickupLocation,
      ParcelState::kOutForDelivery, ParcelState::kDeliveryFailed,
      ParcelState::kError,          ParcelState::kCancelled,
      ParcelState::kReturnToSender, ParcelState::kReturnCompleted};
  base::Time estimated_delivery_date = base::Time::Now() + base::Days(5);

  for (const auto& state : states) {
    ParcelTrackingItem* item =
        createParcelTrackingItem(state, estimated_delivery_date);
    ParcelTrackingModuleView* view =
        [[ParcelTrackingModuleView alloc] initWithFrame:CGRectZero];
    [view configureView:item];
    NSString* title_with_estimated_delivery_date =
        view.titleLabelTextForTesting;

    item = createParcelTrackingItem(state, /*estimated_delivery=*/std::nullopt);
    view = [[ParcelTrackingModuleView alloc] initWithFrame:CGRectZero];
    [view configureView:item];
    NSString* title_without_estimated_delivery_date =
        view.titleLabelTextForTesting;

    EXPECT_NSEQ(title_with_estimated_delivery_date,
                title_without_estimated_delivery_date)
        << "(state = " << parcelStateToString(state) << ")";
  }
}
