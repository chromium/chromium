// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_INSTALL_ATTRIBUTION_INSTALL_ATTRIBUTION_HELPER_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_INSTALL_ATTRIBUTION_INSTALL_ATTRIBUTION_HELPER_H_

// This helper contains some basic logic to report app install attribution from
// external promo campaigns.
namespace install_attribution {

// The time interval buckets between acceptance of an external promo and the
// app install.
//
// LINT.IfChange(InstallAttributionType)
enum InstallAttributionType {
  None = 0,
  Within24Hours = 1,
  Within15Days = 2,
  kMaxValue = Within15Days,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSInstallAttributionType)

// Reads from the app group user defaults to see if there is any external promo
// acceptance data. If there is, and the stored timestamp is within the limit,
// records campaign identifier metrics.
void LogInstallAttribution();

}  // namespace install_attribution

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_INSTALL_ATTRIBUTION_INSTALL_ATTRIBUTION_HELPER_H_
