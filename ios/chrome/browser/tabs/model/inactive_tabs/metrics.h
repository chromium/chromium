// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_METRICS_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_METRICS_H_

// UMA histogram names.
extern const char kInactiveTabsThresholdSettingHistogram[];

// Enum for the IOS.InactiveTabs.Settings histogram.
// Keep in sync with "InactiveTabsThresholdSettingType"
// in src/tools/metrics/histograms/enums.xml.
enum class InactiveTabsThresholdSetting {
  kUnknown = 0,       // Never logged.
  kNeverMove = 1,     // The user disabled the feature.
  kOneWeek = 2,       // The user chose One Week option.
  kTwoWeeks = 3,      // The user chose Two Weeks option.
  kThreeWeeks = 4,    // The user chose Three Weeks option.
  kDefaultValue = 5,  // The user never changed the settings.
  kMaxValue = kDefaultValue,
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_METRICS_H_
