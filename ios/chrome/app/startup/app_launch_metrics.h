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

// Values of the UMA Startup.MobileSessionCallerApp histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum MobileSessionCallerApp {
  CALLER_APP_GOOGLE_SEARCH = 0,
  CALLER_APP_GOOGLE_GMAIL = 1,
  CALLER_APP_GOOGLE_PLUS = 2,
  CALLER_APP_GOOGLE_DRIVE = 3,
  CALLER_APP_GOOGLE_EARTH = 4,
  CALLER_APP_GOOGLE_OTHER = 5,
  CALLER_APP_OTHER = 6,
  CALLER_APP_APPLE_MOBILESAFARI = 7,
  CALLER_APP_APPLE_OTHER = 8,
  CALLER_APP_GOOGLE_YOUTUBE = 9,
  CALLER_APP_GOOGLE_MAPS = 10,
  // Includes being launched from Smart App Banner.
  CALLER_APP_NOT_AVAILABLE = 11,
  CALLER_APP_GOOGLE_CHROME_TODAY_EXTENSION = 12,
  CALLER_APP_GOOGLE_CHROME_SEARCH_EXTENSION = 13,
  CALLER_APP_GOOGLE_CHROME_CONTENT_EXTENSION = 14,
  CALLER_APP_GOOGLE_CHROME_SHARE_EXTENSION = 15,
  CALLER_APP_GOOGLE_CHROME = 16,
  // An application launched Chrome with an http/https URL as the default
  // browser.
  CALLER_APP_THIRD_PARTY = 17,
  CALLER_APP_GOOGLE_CHROME_OPEN_EXTENSION = 18,
  MOBILE_SESSION_CALLER_APP_COUNT,
};

#endif  // IOS_CHROME_APP_STARTUP_APP_LAUNCH_METRICS_H_
