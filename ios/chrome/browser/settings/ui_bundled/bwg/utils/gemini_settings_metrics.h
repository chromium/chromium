// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UTILS_GEMINI_SETTINGS_METRICS_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UTILS_GEMINI_SETTINGS_METRICS_H_

// Enum for the IOS.Gemini.Settings histogram.
// LINT.IfChange(IOSGeminiSettingsItem)
enum class IOSGeminiSettingsItem {
  kGeminiAppsActivity = 0,
  kPersonalization = 1,
  kExtensions = 2,
  kUnknown = 3,
  kMaxValue = kUnknown,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiSettingsItem)

// Records that the user opened the Gemini settings page.
void RecordGeminiSettingsOpened();

// Records that the user tapped the close button on the Gemini settings page.
void RecordGeminiSettingsClose();

// Records that the user tapped the back button on the Gemini settings page.
void RecordGeminiSettingsBack();

// Records that the user tapped on the Gemini Apps Activity settings item.
void RecordGeminiSettingsAppsActivity();

// Records that the user tapped on the Gemini Extensions settings item.
void RecordGeminiSettingsExtensions();

// Records that the user tapped on the Gemini Personalization settings item.
void RecordGeminiSettingsPersonalization();

// Records that an item is shown in the Gemini settings page.
void RecordGeminiSettingsItemShown(IOSGeminiSettingsItem item);

// Records that the user tapped an item in the Gemini settings page.
void RecordGeminiSettingsItemUsed(IOSGeminiSettingsItem item);

// Records that the user tapped the close button on the Gemini camera settings
// page.
void RecordGeminiCameraSettingsClose();

// Records that the user tapped the back button on the Gemini camera settings
// page.
void RecordGeminiCameraSettingsBack();

// Records that the user toggled the Gemini camera setting on/off on the Gemini
// camera settings page.
void RecordGeminiCameraSettingsToggled(bool enabled);

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UTILS_GEMINI_SETTINGS_METRICS_H_
