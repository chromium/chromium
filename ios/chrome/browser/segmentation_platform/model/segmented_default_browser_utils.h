// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_UTILS_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_UTILS_H_

#import "base/time/time.h"

namespace segmentation_platform {

struct ClassificationResult;

// Maximum time to wait to retrieve a result from the
// deviceSwitcherResultDispatcher before returning PredictionStatus kNotReady.
extern const base::TimeDelta kDeviceSwitcherWaitTimeout;

// User classifications from the Segmentation
// Platform used by the Segmented Default Browser promo.
enum class DefaultBrowserUserSegment {
  // Classification for users of Chrome on Desktop.
  kDesktopUser,
  // Classification for users switching from Chrome on Android to Chrome on iOS.
  kAndroidSwitcher,
  // Classification for users who use Chrome shopping features.
  kShopper,
  // Classification for users not identified as part of kDesktopUser,
  // kAndroidSwitcher, or kShopper
  kDefault
};

// Utility function to determine the DefaultBrowserUserSegment based on user's
// device switcher ClassificationResult.
DefaultBrowserUserSegment GetDefaultBrowserUserSegment(
    const ClassificationResult& device_switcher_result);

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_UTILS_H_
