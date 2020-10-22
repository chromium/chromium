// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_LEGACY_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_LEGACY_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_

#include "components/password_manager/core/browser/password_form_forward.h"
#import "ios/chrome/browser/ui/settings/password/legacy_password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol ReauthenticationProtocol;

// TODO(crbug.com/1096986): Delete this view controller after Password Check
// launch.

// Displays details of a password item, including URL of the site, username and
// password in masked state as default. User can copy the URL and username,
// pass the iOS security check to see and copy the password , or delete the
// password item.
@interface LegacyPasswordDetailsTableViewController
    : SettingsRootTableViewController

// The designated initializer.
- (nullable instancetype)
      initWithPasswordForm:(const password_manager::PasswordForm&)passwordForm
                  delegate:
                      (nonnull
                           id<LegacyPasswordDetailsTableViewControllerDelegate>)
                          delegate
    reauthenticationModule:
        (nonnull id<ReauthenticationProtocol>)reauthenticationModule
    NS_DESIGNATED_INITIALIZER;

- (nullable instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_LEGACY_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_
