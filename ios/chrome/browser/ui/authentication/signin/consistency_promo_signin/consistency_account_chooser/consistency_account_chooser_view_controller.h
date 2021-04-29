// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/child_bottom_sheet_view_controller.h"

@protocol ConsistencyAccountChooserTableViewControllerActionDelegate;
@protocol ConsistencyAccountChooserTableViewControllerModelDelegate;
@protocol ConsistencyAccountChooserConsumer;

// View controller for ConsistencyAccountChooserCoordinator.
@interface ConsistencyAccountChooserViewController
    : UIViewController <ChildBottomSheetViewController>

@property(nonatomic, weak)
    id<ConsistencyAccountChooserTableViewControllerActionDelegate>
        actionDelegate;
@property(nonatomic, weak)
    id<ConsistencyAccountChooserTableViewControllerModelDelegate>
        modelDelegate;
@property(nonatomic, strong, readonly) id<ConsistencyAccountChooserConsumer>
    consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_
