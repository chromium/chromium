// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_FEATURES_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_FEATURES_H_

#import "base/feature_list.h"

// Enables the EnableScreenshotProtectionIOS feature.
BASE_DECLARE_FEATURE(kEnableScreenshotProtectionIOS);

// Returns true if the EnableScreenshotProtectionIOS feature is enabled.
bool IsEnableScreenshotProtectionIOSEnabled();

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_FEATURES_H_
