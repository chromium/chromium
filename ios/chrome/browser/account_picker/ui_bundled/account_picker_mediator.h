// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol AccountPickerMediatorDelegate;
class AuthenticationService;

// Mediator for AccountPicker.
@interface AccountPickerMediator : NSObject

@property(nonatomic, weak) id<AccountPickerMediatorDelegate> delegate;

- (instancetype)initWithAuthenticationService:
    (AuthenticationService*)authenticationService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_MEDIATOR_H_
