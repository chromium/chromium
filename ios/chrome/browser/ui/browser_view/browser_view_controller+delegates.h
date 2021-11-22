// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_DELEGATES_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_DELEGATES_H_

#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/common_tab_helper_delegate.h"

@class DownloadManagerCoordinator;
@class SideSwipeController;
@class SadTabCoordinator;

// Category on BrowserViewController that exposes some internal controllers and
// coordinators that are used to set up tab helper delegates. This category also
// declares the BVC's conformance to several tab helper delegates (defined in
// common_tab_helper_delegate.h) which are also used to set up tab helper
// delegates.
// This category is scaffolding for refactoring these delegate responsibilities
// out of the BVC; its use should be limited, and the goal is to remove
// properties and protocols from it (and from the BVC).
@interface BrowserViewController (Delegates) <CommonTabHelperDelegate>

@property(nonatomic, strong, readonly) SideSwipeController* sideSwipeController;

// TODO(crbug.com/1272494): Move this to BrowserCoordinator.
@property(nonatomic, strong, readonly) SadTabCoordinator* sadTabCoordinator;

// TODO(crbug.com/1272495): Move this to BrowserCoordinator.
@property(nonatomic, strong, readonly)
    DownloadManagerCoordinator* downloadManagerCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_DELEGATES_H_
