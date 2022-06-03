// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class FindBarControllerIOS;
@class FindBarCoordinator;
@class ToolbarAccessoryPresenter;
@protocol ToolbarAccessoryCoordinatorDelegate;

@protocol FindBarPresentationDelegate

- (void)setHeadersForFindBarCoordinator:(FindBarCoordinator*)findBarCoordinator;

@end

// Coordinator for the Find Bar and the Find In page feature. Currently, this
// is mostly a collection of code extracted from BrowserViewController and not
// a good example of the ideal coordinator architecture.
@interface FindBarCoordinator : ChromeCoordinator

// Presenter used to present the UI.
@property(nonatomic, strong) ToolbarAccessoryPresenter* presenter;

@property(nonatomic, weak) id<ToolbarAccessoryCoordinatorDelegate> delegate;

@property(nonatomic, weak) id<FindBarPresentationDelegate> presentationDelegate;

// Find bar controller object. This should probably be private, but is not to
// make the transition easier.
@property(nonatomic, strong) FindBarControllerIOS* findBarController;

// Defocuses the Find Bar text field.
- (void)defocusFindBar;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_COORDINATOR_H_
