// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_LOGGER_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_LOGGER_H_

#import <Foundation/Foundation.h>

// Protocol for features presenting the account picker to log events.
@protocol AccountPickerLogger <NSObject>

// Called when the account picker list is opened.
- (void)logAccountPickerSelectionScreenOpened;

// Called when the user selected an identity in the account picker selection
// screen, that is different from the initially selected identity.
- (void)logAccountPickerNewIdentitySelected;

// Called when the account picker list is closed using the "Back" button.
- (void)logAccountPickerSelectionScreenClosed;

// Called when the user Add Account UI is opened.
- (void)logAccountPickerAddAccountScreenOpened;

// Called when the user completed adding an account on the device using the
// account picker.
- (void)logAccountPickerAddAccountCompleted;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_LOGGER_H_
