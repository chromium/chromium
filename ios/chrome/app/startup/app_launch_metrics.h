// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_APP_LAUNCH_METRICS_H_
#define IOS_CHROME_APP_STARTUP_APP_LAUNCH_METRICS_H_

// Name of the histogram that records the app launch source.
const char kAppLaunchSource[] = "IOS.LaunchSource";

// Values of the UMA IOS.LaunchSource histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange
enum class AppLaunchSource {
  APP_ICON = 0,
  LONG_PRESS_ON_APP_ICON = 1,
  WIDGET = 2,
  SPOTLIGHT_CHROME = 3,
  LINK_OPENED_FROM_OS = 4,
  LINK_OPENED_FROM_APP = 5,
  SIRI_SHORTCUT = 6,
  X_CALLBACK = 7,
  HANDOFF = 8,
  NOTIFICATION = 9,
  EXTERNAL_ACTION = 10,
  kMaxValue = EXTERNAL_ACTION,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

#endif  // IOS_CHROME_APP_STARTUP_APP_LAUNCH_METRICS_H_
