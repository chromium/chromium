// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_DELEGATES_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_DELEGATES_H_

#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/common_tab_helper_delegate.h"

@class DownloadManagerCoordinator;
@class SideSwipeMediator;

// Category on BrowserViewController that that exposes some internal controllers
// and coordinators that are used to set up tab helper delegates. This category
// also declares the BVC's conformance to several tab helper delegate protocols
// (enumerated in common_tab_helper_delegate.h) which are used to set up tab
// helpers. This category is scaffolding for refactoring these delegate
// responsibilities out of the BVC; its use should be limited, and the goal is
// to remove properties and protocols from it (and from the BVC).
@interface BrowserViewController (Delegates) <CommonTabHelperDelegate>

@property(nonatomic, strong, readonly) SideSwipeMediator* sideSwipeMediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_DELEGATES_H_
