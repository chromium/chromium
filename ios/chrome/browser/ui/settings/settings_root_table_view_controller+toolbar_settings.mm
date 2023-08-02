// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller+toolbar_settings.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation SettingsRootTableViewController (ToolbarSettings)

- (UIBarButtonItem*)settingsButtonWithAction:(SEL)action {
  UIBarButtonItem* settingsButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_TOOLBAR_SETTINGS_SUBMENU)
              style:UIBarButtonItemStylePlain
             target:self
             action:action];
  settingsButton.accessibilityIdentifier = kSettingsToolbarSettingsButtonId;
  return settingsButton;
}

@end
