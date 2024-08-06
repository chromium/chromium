// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"

enum class AccountErrorUserActionableType;
@class IdentityViewItem;
@protocol SystemIdentity;

// Consumer protocol for accounts.
@protocol AccountsConsumer <NSObject>

// Reloads all items. Does nothing if the model is not loaded yet.
- (void)reloadAllItems;

// Updates error section.
- (void)updateErrorSectionModelAndReloadViewIfNeeded:(BOOL)reloadViewIfNeeded;

// Pops the view.
- (void)popView;

@optional

// TODO(crbug.com/349071402): Introduce separate consumers for each of the
// legacy and the new view controllers. One of the following two methods needs
// to be implemented.
// Updates account identity.
- (void)updateAccountIdentity:(id<SystemIdentity>)identity;
// Updates identity view item.
- (void)updateIdentityViewItem:(IdentityViewItem*)identityViewItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_CONSUMER_H_
