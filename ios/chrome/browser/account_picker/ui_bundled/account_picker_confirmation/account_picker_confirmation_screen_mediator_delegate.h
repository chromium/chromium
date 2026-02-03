// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_MEDIATOR_DELEGATE_H_

@class AccountPickerConfirmationScreenMediator;

@protocol AccountPickerConfirmationScreenMediatorDelegate <NSObject>

// The mediators informs the delegate that sign-in is not possible anymore.
- (void)accountPickerConfirmationScreenMediatorWantsToBeStopped:
    (AccountPickerConfirmationScreenMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_MEDIATOR_DELEGATE_H_
