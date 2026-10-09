// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_settings_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class GeminiSettingsViewController;
@protocol GeminiSettingsMutator;

// Delegate for settings dismissal events requested by child view controllers.
@protocol GeminiSettingsDismissalDelegate <NSObject>
- (void)settingsViewControllerDidRequestDismissal:
    (UIViewController*)viewController;
@end

// Delegate for presentation events related to `GeminiSettingsViewController`.
@protocol GeminiSettingsViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)geminiSettingsViewControllerWasRemoved:
    (GeminiSettingsViewController*)controller;

// Called when the user selects the Gemini suggestions row.
- (void)geminiSettingsViewControllerDidSelectSuggestions:
    (GeminiSettingsViewController*)controller;

@end

// View controller related to Gemini setting.
@interface GeminiSettingsViewController
    : SettingsRootTableViewController <GeminiSettingsConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak) id<GeminiSettingsViewControllerPresentationDelegate>
    presentationDelegate;
@property(nonatomic, weak) id<GeminiSettingsMutator> mutator;
@property(nonatomic, weak) id<GeminiSettingsDismissalDelegate>
    geminiSettingsDismissalDelegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_SETTINGS_VIEW_CONTROLLER_H_
