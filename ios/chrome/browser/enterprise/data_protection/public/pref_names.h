// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PUBLIC_PREF_NAMES_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PUBLIC_PREF_NAMES_H_

class PrefRegistrySimple;

namespace enterprise_data_protection {

// Dictionary preference that maps WebStateIDs to the watermark text string to
// display for the tab.
extern const char kDataProtectionWatermarkedTabs[];

// Boolean preference set by the DisableScreenshots enterprise policy.
// TODO(crbug.com/582326248): Consolidate `kDisableScreenshots` into
// `policy::policy_prefs::kDisableScreenshots` across platforms.
inline constexpr char kDisableScreenshots[] = "disable_screenshots";

// Registers the profile prefs for the data protection component.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_data_protection

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PUBLIC_PREF_NAMES_H_
