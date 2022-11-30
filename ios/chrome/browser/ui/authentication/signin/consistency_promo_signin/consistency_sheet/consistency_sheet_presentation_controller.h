// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CONSISTENCY_SHEET_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CONSISTENCY_SHEET_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class ConsistencySheetNavigationController;

// Presentation controller to present ConsistencySheetNavigationController from
// the bottom of the screen. Related to ConsistencySheetNavigationController.
@interface ConsistencySheetPresentationController : UIPresentationController

- (instancetype)initWithConsistencySheetNavigationController:
                    (ConsistencySheetNavigationController*)viewcontroller
                                    presentingViewController:
                                        (UIViewController*)
                                            presentingViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CONSISTENCY_SHEET_PRESENTATION_CONTROLLER_H_
