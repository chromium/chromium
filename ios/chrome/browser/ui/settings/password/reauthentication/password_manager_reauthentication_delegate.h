// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_PASSWORD_MANAGER_REAUTHENTICATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_PASSWORD_MANAGER_REAUTHENTICATION_DELEGATE_H_

// Delegate handling local authentication events for Password Manager surfaces.
@protocol PasswordManagerReauthenticationDelegate <NSObject>

// Dismisses the Password Manager and any presented UI after the user failed to
// pass local authentication while in any of the Password Manager surfaces.
- (void)dismissPasswordManagerAfterFailedReauthentication;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_PASSWORD_MANAGER_REAUTHENTICATION_DELEGATE_H_
