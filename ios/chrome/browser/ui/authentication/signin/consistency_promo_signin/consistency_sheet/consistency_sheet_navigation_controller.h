// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CONSISTENCY_SHEET_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CONSISTENCY_SHEET_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Navigation controller presented from the bottom. The pushed view controllers
// view have to be UIScrollView. This is required to support high font size
// (related to accessibility) with small devices (like iPhone SE).
// The view is automatically sized according to the last child view controller.
// This class works with ConsistencySheetPresentationController and
// ConsistencySheetSlideTransitionAnimator.
// Child view controller are required to implement
// ChildConsistencySheetViewController protocol.
@interface ConsistencySheetNavigationController : UINavigationController

// Returns the desired size related to the current view controller shown by
// |ConsistencySheetNavigationController|.
- (CGSize)layoutFittingSize;

// Updates internal views according to the consistency sheet view position.
- (void)didUpdateControllerViewFrame;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CONSISTENCY_SHEET_NAVIGATION_CONTROLLER_H_
