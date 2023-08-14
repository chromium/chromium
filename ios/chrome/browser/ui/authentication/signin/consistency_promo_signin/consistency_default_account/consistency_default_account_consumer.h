// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "components/sync/base/user_selectable_type.h"

// Consumer for consistency default account.
@protocol ConsistencyDefaultAccountConsumer <NSObject>

// Informs the consumer whether the sync-transport layer got completely nuked
// by the SyncDisabled policy. Notice this is different from disabling all types
// via the SyncTypesListDisabled policy. The latter maps to the
// user-controllable toggles (syncer::UserSelectableType) but some functionality
// isn't gated behind those toggles, e.g. send-tab-to-self. Those features would
// be disabled by SyncDisabled but not SyncTypesListDisabled. All that to say:
// this setter can't be bundled with setSyncTypesDisabledByPolicy below.
- (void)setSyncTransportDisabledByPolicy:(BOOL)disabled;

// Informs the consumer whether individual sync types got disabled by the
// SyncTypesListDisabled enterprise policy. See also the comment in
// setSyncTransportDisabledByPolicy.
- (void)setSyncTypesDisabledByPolicy:(syncer::UserSelectableTypeSet)types;

// Updates the user information, and show the default account.
- (void)showDefaultAccountWithFullName:(NSString*)fullName
                             givenName:(NSString*)givenName
                                 email:(NSString*)email
                                avatar:(UIImage*)avatar;

// Disable display for the default account button, for when an account isn't
// available on the device.
- (void)hideDefaultAccount;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_CONSUMER_H_
