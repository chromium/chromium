// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_COORDINATOR_WATERMARK_STYLE_UTILS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_COORDINATOR_WATERMARK_STYLE_UTILS_H_

#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view_utils.h"

class PrefService;

// Returns the `WatermarkStyle` populated with values from `pref_service`,
// with values clamped to safe ranges:
// - fill_opacity: clamped to [0, 100]%, converted to a fraction in [0.0, 1.0].
// - outline_opacity: clamped to [0, 100]%, converted to a fraction in
// [0.0, 1.0].
// - font_size: clamped to [1, 500]pt.
WatermarkStyle GetWatermarkStyleFromPrefs(const PrefService* pref_service);

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_COORDINATOR_WATERMARK_STYLE_UTILS_H_
