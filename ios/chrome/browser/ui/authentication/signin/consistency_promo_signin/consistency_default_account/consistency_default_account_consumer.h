// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer for consistency default account.
@protocol ConsistencyDefaultAccountConsumer <NSObject>

// Updates the user information.
- (void)updateWithFullName:(NSString*)fullName
                 givenName:(NSString*)givenName
                     email:(NSString*)email;

// Updates the user avatar.
- (void)updateUserAvatar:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_CONSUMER_H_
