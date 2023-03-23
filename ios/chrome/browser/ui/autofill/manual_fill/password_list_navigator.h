// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_LIST_NAVIGATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_LIST_NAVIGATOR_H_

// Object to navigate different views in manual fallback's passwords list.
@protocol PasswordListNavigator

// Requests to open the list of all passwords.
- (void)openAllPasswordsList;

// Opens password manager.
- (void)openPasswordManager;

// Opens passwords settings.
- (void)openPasswordSettings;

// Opens password suggestion.
- (void)openPasswordSuggestion;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_LIST_NAVIGATOR_H_
