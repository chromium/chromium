// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "components/sync/base/user_selectable_type.h"

// Consumer for consistency default account.
@protocol ConsistencyDefaultAccountConsumer <NSObject>

// Sets the label text. It's fine to pass nil if there's supposed to be none.
- (void)setLabelText:(NSString*)text;

// Sets the text in the button that aborts the flow.
- (void)setSkipButtonText:(NSString*)text;

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
