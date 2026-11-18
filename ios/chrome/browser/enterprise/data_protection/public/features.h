// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PUBLIC_FEATURES_H_

#import "base/feature_list.h"

// Kill switch for enforcing the DisableScreenshots policy on iOS.
BASE_DECLARE_FEATURE(kDisableScreenshotsPolicyIOS);

// Returns true if enforcing the DisableScreenshots policy on iOS is enabled.
bool IsDisableScreenshotsPolicyIOSEnabled();

// Enables the enterprise watermarking feature on iOS.
BASE_DECLARE_FEATURE(kEnableEnterpriseWatermarkingIOS);

// Returns true if the EnableEnterpriseWatermarkingIOS feature is enabled.
bool IsEnableEnterpriseWatermarkingIOS();

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PUBLIC_FEATURES_H_
