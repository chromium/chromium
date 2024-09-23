// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAD_TAB_UI_BUNDLED_SAD_TAB_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SAD_TAB_UI_BUNDLED_SAD_TAB_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/web/model/sad_tab_tab_helper_delegate.h"

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

// Disconnects all delegates set by the coordinator on any web states in its web
// state list. After `disconnect` is called, the coordinator will not add
// delegates to further webstates.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SAD_TAB_UI_BUNDLED_SAD_TAB_COORDINATOR_H_
