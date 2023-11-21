// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_SETTINGS_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_SETTINGS_PRESENTER_H_

// Protocol used to display the account settings UI of the signed in account.
@protocol AccountSettingsPresenter

// Asks the presenter to display account settings.
- (void)showAccountSettings;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_SETTINGS_PRESENTER_H_
