// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller.h"

// Testing category to expose a private method of PasswordManagerViewController
// for tests.
@interface PasswordManagerViewController (Testing) <PasswordsConsumer,
                                                    UISearchBarDelegate,
                                                    UISearchControllerDelegate>
- (BOOL)didReceivePasswords;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_TESTING_H_
