// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAD_TAB_SAD_TAB_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SAD_TAB_SAD_TAB_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper_delegate.h"

@protocol OverscrollActionsControllerDelegate;

// Coordinator that displays a SadTab view.
@interface SadTabCoordinator : ChromeCoordinator<SadTabTabHelperDelegate>

// Required to support Overscroll Actions UI, which is displayed when Sad Tab is
// pulled down.
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollDelegate;

@property(nonatomic, readonly) UIViewController* viewController;

// YES if page load for this URL has failed more than once.
@property(nonatomic) BOOL repeatedFailure;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAD_TAB_SAD_TAB_COORDINATOR_H_
