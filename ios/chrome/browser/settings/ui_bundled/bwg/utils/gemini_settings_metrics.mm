// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/utils/gemini_settings_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"

void RecordGeminiSettingsOpened() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiSettingsOpened"));
}

void RecordGeminiSettingsClose() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiSettingsClose"));
}

void RecordGeminiSettingsBack() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiSettingsBack"));
}

void RecordGeminiSettingsAppsActivity() {
  base::RecordAction(
      base::UserMetricsAction("Settings.GeminiSettings.GeminiAppsActivity"));
}

void RecordGeminiSettingsExtensions() {
  base::RecordAction(
      base::UserMetricsAction("Settings.GeminiSettings.GeminiExtensions"));
}

void RecordGeminiSettingsPersonalization() {
  base::RecordAction(
      base::UserMetricsAction("Settings.GeminiSettings.GeminiPersonalization"));
}

void RecordGeminiSettingsItemShown(IOSGeminiSettingsItem item) {
  base::UmaHistogramEnumeration("IOS.Gemini.DynamicSettings.ItemShown", item);
}

void RecordGeminiSettingsItemUsed(IOSGeminiSettingsItem item) {
  base::UmaHistogramEnumeration("IOS.Gemini.DynamicSettings.ItemUsed", item);
}

void RecordGeminiCameraSettingsClose() {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiCameraSettingsClose"));
}

void RecordGeminiCameraSettingsBack() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiCameraSettingsBack"));
}

void RecordGeminiCameraSettingsToggled(bool enabled) {
  base::UmaHistogramBoolean(
      "IOS.Gemini.Camera.Settings.GeminiCameraPermissionToggled", enabled);
  if (enabled) {
    base::RecordAction(base::UserMetricsAction(
        "MobileGeminiCameraSettingsGeminiCameraPermissionToggledOn"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "MobileGeminiCameraSettingsGeminiCameraPermissionToggledOff"));
  }
}
