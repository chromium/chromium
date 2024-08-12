// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MEDIATOR_DELEGATE_H_

@protocol SystemIdentity;

@protocol AccountsMediatorDelegate <NSObject>

// Called to remove identity.
// TODO(crbug.com/349071402): The identity should not be removed directly, the
// AccountsMediatorDelegate should show a confirmation action sheet.
- (void)handleRemoveIdentity:(id<SystemIdentity>)identity;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MEDIATOR_DELEGATE_H_
