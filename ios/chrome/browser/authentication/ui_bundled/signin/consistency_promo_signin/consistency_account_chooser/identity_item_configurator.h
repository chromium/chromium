// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_IDENTITY_ITEM_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_IDENTITY_ITEM_CONFIGURATOR_H_

#import <UIKit/UIKit.h>

@class TableViewIdentityItem;

// This class configures TableViewIdentityItem instances.
@interface IdentityItemConfigurator : NSObject

@property(nonatomic, copy) NSString* gaiaID;
@property(nonatomic, copy) NSString* name;
@property(nonatomic, copy) NSString* email;
@property(nonatomic, strong) UIImage* avatar;
@property(nonatomic, assign) BOOL selected;
@property(nonatomic, assign) BOOL managed;

- (void)configureIdentityChooser:(TableViewIdentityItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_IDENTITY_ITEM_CONFIGURATOR_H_
