// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_UTILS_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_UTILS_H_

#import "base/time/time.h"
#import "components/segmentation_platform/public/result.h"

namespace segmentation_platform {

// Maximum time to wait to retrieve a result from the
// `DeviceSwitcherResultDispatcher` before returning PredictionStatus kNotReady.
extern const base::TimeDelta kDeviceSwitcherWaitTimeout;

// User classifications from the Segmentation
// Platform used by the Segmented Default Browser promo. Entries should not be
// renumbered, and numeric values should never be reused.
enum class DefaultBrowserUserSegment {
  // Classification for users not identified as part of kDesktopUser,
  // kAndroidSwitcher, or kShopper
  kDefault = 0,
  // Classification for users of Chrome on Desktop.
  kDesktopUser = 1,
  // Classification for users switching from Chrome on Android to Chrome on iOS.
  kAndroidSwitcher = 2,
  // Classification for users who use Chrome shopping features.
  kShopper = 3,
  kMaxValue = kShopper
};

// Utility function to determine the `DefaultBrowserUserSegment` based on user's
// device switcher and shopping user segment `ClassificationResult`. The segment
// returned can be forced through the experimental settings. To force the
// Shopper label, the forced Device Switcher label must be unset.
DefaultBrowserUserSegment GetDefaultBrowserUserSegment(
    const ClassificationResult* device_switcher_result,
    const ClassificationResult* shopper_result);

// Utility function that returns the First Run Experience Default Browser promo
// title's string ID for a specified user segment.
int GetFirstRunDefaultBrowserScreenTitleStringID(
    DefaultBrowserUserSegment segment);

// Utility function that returns the First Run Experience Default Browser promo
// subtitle's string ID for a specified user segment.
int GetFirstRunDefaultBrowserScreenSubtitleStringID(
    DefaultBrowserUserSegment segment);

// Utility function that returns the Default Browser Recurring Video promo
// title's string ID for a specified user segment.
int GetDefaultBrowserGenericPromoTitleStringID(
    DefaultBrowserUserSegment segment);

// Utility function that returns Set Up List Default Browser item description's
// string ID for a specified user segment.
int GetSetUpListDefaultBrowserDescriptionStringID(
    DefaultBrowserUserSegment segment);

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_UTILS_H_
