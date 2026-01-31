// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_ACTION_TYPE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_ACTION_TYPE_H_

#import <Foundation/Foundation.h>

// The type of the settings action.
typedef NS_ENUM(NSInteger, GeminiSettingsActionType) {
  GeminiSettingsActionTypeUnknown = 0,

  // The settings action opens a view controller.
  GeminiSettingsActionTypeViewController = 1,

  // The settings action opens a URL.
  GeminiSettingsActionTypeURL = 2,
};

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_ACTION_TYPE_H_
