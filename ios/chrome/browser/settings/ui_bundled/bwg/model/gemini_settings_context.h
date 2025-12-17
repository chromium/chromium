// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_CONTEXT_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_CONTEXT_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, GeminiSettingsContext) {
  // Settings for managing Gemini apps activity.
  GeminiSettingsContextGeminiAppsActivity = 0,
  // Settings for managing personal data and customization.
  GeminiSettingsContextPersonalization,
  // Settings for managing plugins and extensions.
  GeminiSettingsContextExtensions,
  // Unknown settings context.
  GeminiSettingsContextUnknown,
};

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_CONTEXT_H_
