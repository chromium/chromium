// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_LIST_DELEGATE_H_

// Delegate for actions in manual fallback's passwords list.
@protocol PasswordListDelegate

// Dismisses the presented view controller and continues as pop over on iPads
// or above the keyboard else.
- (void)dismissPresentedViewController;

// Requests to open the list of all passwords.
- (void)openAllPasswordsList;

// Opens passwords settings.
- (void)openPasswordSettings;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_LIST_DELEGATE_H_
