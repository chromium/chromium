// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/child_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@class ConsistencyAccountChooserViewController;

// Delegate protocol for ConsistencyAccountChooserViewController.
@protocol ConsistencyAccountChooserViewControllerActionDelegate <NSObject>

// Invoked when the user selects an identity.
- (void)consistencyAccountChooserViewController:
            (ConsistencyAccountChooserViewController*)viewController
                    didSelectIdentityWithGaiaID:(NSString*)gaiaID;
// Invoked when the user taps on "Add account".
- (void)consistencyAccountChooserViewControllerDidTapOnAddAccount:
    (ConsistencyAccountChooserViewController*)viewController;

@end

// Protocol to get the model.
@protocol ConsistencyAccountChooserViewControllerModelDelegate <NSObject>

// Returns all the configurators to generate model items.
@property(nonatomic, strong, readonly) NSArray* sortedIdentityItemConfigurators;

@end

// View controller for ConsistencyAccountChooserCoordinator.
@interface ConsistencyAccountChooserViewController
    : ChromeTableViewController <ChildBottomSheetViewController,
                                 ConsistencyAccountChooserConsumer>

@property(nonatomic, weak)
    id<ConsistencyAccountChooserViewControllerActionDelegate>
        actionDelegate;
@property(nonatomic, weak)
    id<ConsistencyAccountChooserViewControllerModelDelegate>
        modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_
