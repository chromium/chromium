// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_APP_LAUNCH_METRICS_H_
#define IOS_CHROME_APP_STARTUP_APP_LAUNCH_METRICS_H_

// Name of the histogram that records the app launch source.
const char kAppLaunchSource[] = "IOS.LaunchSource";

// UMA histogram key for IOS.ExternalAction.
const char kExternalActionHistogram[] = "IOS.ExternalAction";

// UMA histogram key for IOS.WidgetKit.Action.
const char kWidgetKitActionHistogram[] = "IOS.WidgetKit.Action";

// Key of the UMA Startup.MobileSessionStartAction histogram.
const char kUMAMobileSessionStartActionHistogram[] =
    "Startup.MobileSessionStartAction";

// UMA histogram key for iOS.SearchExtension.Action.
const char kSearchExtensionActionHistogram[] = "iOS.SearchExtension.Action";

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

// Values of the UMA IOS.WidgetKit.Action histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange
enum class WidgetKitExtensionAction {
  ACTION_DINO_WIDGET_GAME = 0,
  ACTION_SEARCH_WIDGET_SEARCH = 1,
  ACTION_QUICK_ACTIONS_SEARCH = 2,
  ACTION_QUICK_ACTIONS_INCOGNITO = 3,
  ACTION_QUICK_ACTIONS_VOICE_SEARCH = 4,
  ACTION_QUICK_ACTIONS_QR_READER = 5,
  ACTION_LOCKSCREEN_LAUNCHER_SEARCH = 6,
  ACTION_LOCKSCREEN_LAUNCHER_INCOGNITO = 7,
  ACTION_LOCKSCREEN_LAUNCHER_VOICE_SEARCH = 8,
  ACTION_LOCKSCREEN_LAUNCHER_GAME = 9,
  ACTION_QUICK_ACTIONS_LENS = 10,
  ACTION_SHORTCUTS_SEARCH = 11,
  ACTION_SHORTCUTS_OPEN = 12,
  ACTION_SEARCH_PASSWORDS_WIDGET_SEARCH_PASSWORDS = 13,
  kMaxValue = ACTION_SEARCH_PASSWORDS_WIDGET_SEARCH_PASSWORDS,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Values of the UMA Startup.MobileSessionStartAction histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange
enum MobileSessionStartAction {
  // Logged when an application passes an http URL to Chrome using the custom
  // registered scheme (f.e. googlechrome).
  START_ACTION_OPEN_HTTP = 0,
  // Logged when an application passes an https URL to Chrome using the custom
  // registered scheme (f.e. googlechromes).
  START_ACTION_OPEN_HTTPS = 1,
  START_ACTION_OPEN_FILE = 2,
  START_ACTION_XCALLBACK_OPEN = 3,
  START_ACTION_XCALLBACK_OTHER = 4,
  START_ACTION_OTHER = 5,
  START_ACTION_XCALLBACK_APPGROUP_COMMAND = 6,
  // Logged when any application passes an http URL to Chrome using the standard
  // "http" scheme. This can happen when Chrome is set as the default browser
  // on iOS 14+ as http openURL calls will be directed to Chrome by the system
  // from all other apps.
  START_ACTION_OPEN_HTTP_FROM_OS = 7,
  // Logged when any application passes an https URL to Chrome using the
  // standard "https" scheme. This can happen when Chrome is set as the default
  // browser on iOS 14+ as http openURL calls will be directed to Chrome by the
  // system from all other apps.
  START_ACTION_OPEN_HTTPS_FROM_OS = 8,
  START_ACTION_WIDGET_KIT_COMMAND = 9,
  // Logged when Chrome is opened via the external action scheme.
  START_EXTERNAL_ACTION = 10,
  MOBILE_SESSION_START_ACTION_COUNT
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Values of the UMA iOS.SearchExtension.Action histogram.
// LINT.IfChange
enum SearchExtensionAction {
  ACTION_NO_ACTION,
  ACTION_NEW_SEARCH,
  ACTION_NEW_INCOGNITO_SEARCH,
  ACTION_NEW_VOICE_SEARCH,
  ACTION_NEW_QR_CODE_SEARCH,
  ACTION_OPEN_URL,
  ACTION_SEARCH_TEXT,
  ACTION_SEARCH_IMAGE,
  ACTION_LENS,
  SEARCH_EXTENSION_ACTION_COUNT,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Values of the UMA IOS.ExternalAction histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange
enum class IOSExternalAction {
  // Logged when Chrome is passed an invalid action.
  ACTION_INVALID = 0,
  // Logged when Chrome is passed a "OpenNTP" action.
  ACTION_OPEN_NTP = 1,
  // Logged when Chrome is passed a "DefaultBrowserSettings" action.
  ACTION_DEFAULT_BROWSER_SETTINGS = 2,
  // Logged when Chrome is passed a "DefaultBrowserSettings" action, but instead
  // will show the NTP, since Chrome is already set as default browser.
  ACTION_SKIPPED_DEFAULT_BROWSER_SETTINGS_FOR_NTP = 3,
  kMaxValue = ACTION_SKIPPED_DEFAULT_BROWSER_SETTINGS_FOR_NTP,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

#endif  // IOS_CHROME_APP_STARTUP_APP_LAUNCH_METRICS_H_
