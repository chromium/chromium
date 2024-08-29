// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MEDIATOR_DELEGATE_H_

@protocol SystemIdentity;

@protocol AccountsMediatorDelegate <NSObject>

// Called to remove identity.
- (void)handleRemoveIdentity:(id<SystemIdentity>)identity
                    itemView:(UIView*)itemView;

// Called to show SigninCommand with operation to add account to device.
- (void)showAddAccountToDevice;

// Called to sign out.
- (void)signOutWithItemView:(UIView*)itemView;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MEDIATOR_DELEGATE_H_
