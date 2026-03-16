// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ACCOUNT_SETTINGS_PRESENTER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ACCOUNT_SETTINGS_PRESENTER_H_

// Protocol used to display the account settings UI of the signed in account.
@protocol AccountSettingsPresenter

// Asks the presenter to display account settings.
// The user must be signed-in and sign-in must be enabled.
- (void)showAccountSettings;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ACCOUNT_SETTINGS_PRESENTER_H_
