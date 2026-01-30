// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_BWG_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_BWG_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_settings_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@protocol GeminiSettingsMutator;

// View controller related to Gemini setting.
@interface BWGSettingsViewController
    : SettingsRootTableViewController <GeminiSettingsConsumer,
                                       SettingsControllerProtocol>

@property(nonatomic, weak) id<GeminiSettingsMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_BWG_SETTINGS_VIEW_CONTROLLER_H_
