// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_CONSUMER_H_

#import <Foundation/Foundation.h>

@class IdentityItemConfigurator;

// Consumer for consistency default account.
@protocol ConsistencyAccountChooserConsumer <NSObject>

// Invoked when all identities have to be reloaded.
- (void)reloadAllIdentities;
// Invoked when an identity has to be updated.
- (void)reloadIdentityForIdentityItemConfigurator:
    (IdentityItemConfigurator*)configurator;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_CONSUMER_H_
