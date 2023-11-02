// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_consumer.h"

@protocol ConsistencyAccountChooserTableViewControllerActionDelegate;
@protocol ConsistencyAccountChooserTableViewControllerModelDelegate;

// View controller for ConsistencyAccountChooserCoordinator.
@interface ConsistencyAccountChooserTableViewController
    : LegacyChromeTableViewController <ConsistencyAccountChooserConsumer>

@property(nonatomic, weak)
    id<ConsistencyAccountChooserTableViewControllerActionDelegate>
        actionDelegate;
@property(nonatomic, weak)
    id<ConsistencyAccountChooserTableViewControllerModelDelegate>
        modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_TABLE_VIEW_CONTROLLER_H_
