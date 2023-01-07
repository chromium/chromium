// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_

#import <Foundation/Foundation.h>

@class ConsistencyAccountChooserViewController;

// Protocol to get the model.
@protocol ConsistencyAccountChooserTableViewControllerModelDelegate <NSObject>

// Returns all the configurators to generate model items.
@property(nonatomic, strong, readonly) NSArray* sortedIdentityItemConfigurators;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_
