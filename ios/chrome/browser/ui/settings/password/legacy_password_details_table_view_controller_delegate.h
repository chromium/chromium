// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_LEGACY_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_LEGACY_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

#include "components/password_manager/core/browser/password_form_forward.h"

@class LegacyPasswordDetailsTableViewController;

// PasswordDetailsTableViewController uses this protocol to interact with higher
// level password controller.
@protocol LegacyPasswordDetailsTableViewControllerDelegate

- (void)passwordDetailsTableViewController:
            (LegacyPasswordDetailsTableViewController*)controller
                            deletePassword:
                                (const password_manager::PasswordForm&)
                                    passwordForm;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_LEGACY_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
