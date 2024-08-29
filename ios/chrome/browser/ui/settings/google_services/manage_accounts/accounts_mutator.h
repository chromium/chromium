// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MUTATOR_H_

@protocol AccountsMutator <NSObject>

// Called when remove identity is tapped.
- (void)requestRemoveIdentityWithGaiaID:(NSString*)gaiaID
                               itemView:(UIView*)itemView;

// Called when the user taps to add account from the accounts management UI.
- (void)requestAddIdentityToDevice;

// Called when the user taps to sign out from the accounts management UI.
- (void)requestSignOutWithItemView:(UIView*)itemView;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MUTATOR_H_
