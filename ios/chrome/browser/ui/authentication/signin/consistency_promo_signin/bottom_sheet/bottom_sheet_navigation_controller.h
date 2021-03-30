// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_BOTTOM_SHEET_BOTTOM_SHEET_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_BOTTOM_SHEET_BOTTOM_SHEET_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class BottomSheetNavigationController;

// Delegate protocol for presentation events of BottomSheetNavigationController.
@protocol BottomSheetNavigationControllerPresentationDelegate <NSObject>

// Called when BottomSheetNavigationController disappears. Related to:
// -[UIViewController viewDidDisappear:].
- (void)bottomSheetNavigationControllerDidDisappear:
    (BottomSheetNavigationController*)viewController;

@end

// Navigation controller presented from the bottom. The pushed view controllers
// view have to be UIScrollView. This is required to support high font size
// (related to accessibility) with small devices (like iPhone SE).
// The view is automatically sized according to the last child view controller.
// This class works with BottomSheetPresentationController and
// BottomSheetSlideTransitionAnimator.
// Child view controller are required to implement
// ChildBottomSheetViewController protocol.
@interface BottomSheetNavigationController : UINavigationController

// Presentation delegate.
@property(nonatomic, weak)
    id<BottomSheetNavigationControllerPresentationDelegate>
        presentationDelegate;

// Returns the desired size related to the current view controller shown by
// |BottomSheetNavigationController|.
- (CGSize)layoutFittingSize;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_BOTTOM_SHEET_BOTTOM_SHEET_NAVIGATION_CONTROLLER_H_
