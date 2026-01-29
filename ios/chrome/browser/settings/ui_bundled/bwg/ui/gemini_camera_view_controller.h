// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_CAMERA_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_CAMERA_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@protocol GeminiSettingsMutator;

// View controller related to the Gemini camera setting.
@interface GeminiCameraViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// Used for sending model data updates to the mediator.
@property(nonatomic, weak) id<GeminiSettingsMutator> mutator;

// Whether the camera is enabled.
@property(nonatomic) BOOL cameraEnabled;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_CAMERA_VIEW_CONTROLLER_H_
