// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller+toolbar_add.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SettingsRootTableViewController (ToolbarAdd)

- (UIBarButtonItem*)addButtonWithAction:(SEL)action {
  UIBarButtonItem* addButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_SETTINGS_TOOLBAR_ADD)
              style:UIBarButtonItemStylePlain
             target:self
             action:action];
  addButton.accessibilityIdentifier = kSettingsToolbarAddButtonId;
  return addButton;
}

@end
