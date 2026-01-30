// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_MEDIATOR_DELEGATE_H_

@class AccountPickerSelectionScreenMediator;

@protocol AccountPickerMediatorDelegate <NSObject>

// The mediators informs the delegate that sign-in is not possible anymore.
- (void)accountPickerMediatorWantsToBeStopped:(AccountPickerMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_MEDIATOR_DELEGATE_H_
