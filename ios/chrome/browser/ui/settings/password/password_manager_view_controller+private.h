// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_PRIVATE_H_

// Class extension exposing private methods of PasswordManagerViewController for
// testing.
@interface PasswordManagerViewController () <PasswordsConsumer,
                                             UISearchBarDelegate,
                                             UISearchControllerDelegate>

- (void)updateExportPasswordsButton;

- (BOOL)didReceivePasswords;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_PRIVATE_H_
