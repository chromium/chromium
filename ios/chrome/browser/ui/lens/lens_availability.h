// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LENS_LENS_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_UI_LENS_LENS_AVAILABILITY_H_

#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"

// Enum representing the possible Lens avaiability statuses on iOS.
// Current values should not be renumbered. Please keep in sync with
// "IOSLensSupportStatus" in src/tools/metrics/histograms/enums.xml.
enum class LensSupportStatus {
  LensSearchSupported = 0,
  NonGoogleSearchEngine = 1,
  DeviceFormFactorTablet = 2,
  DisabledByFlag = 3,
  ProviderUnsupported = 4,
  DisabledByEnterprisePolicy = 5,
  kMaxValue = DisabledByEnterprisePolicy,
};

// Histogram name for the support status of the context menu entrypoint.
extern const char kIOSLensContextMenuSupportStatusHistogramName[];

// Histogram name for the support status of the keyboard entrypoint.
extern const char kIOSLensKeyboardSupportStatusHistogramName[];

// Histogram name for the support status of the new tab page entrypoint.
extern const char kIOSLensNewTabPageSupportStatusHistogramName[];

namespace lens_availability {
// Checks for and performs UMA logging for the availability of the given
// Lens entry point and search engine default provider.
bool CheckAndLogAvailabilityForLensEntryPoint(
    LensEntrypoint entry_point,
    bool is_google_default_search_engine);
}  // namespace lens_availability

#endif  // IOS_CHROME_BROWSER_UI_LENS_LENS_AVAILABILITY_H_
