// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class FindBarControllerIOS;
@class FindBarCoordinator;
@class ToolbarAccessoryPresenter;
@protocol ToolbarAccessoryCoordinatorDelegate;

@protocol FindBarPresentationDelegate

- (void)setHeadersForFindBarCoordinator:(FindBarCoordinator*)findBarCoordinator;

// Called when the Find bar is presented by its presenter.
- (void)findBarDidAppearForFindBarCoordinator:
    (FindBarCoordinator*)findBarCoordinator;

// Called when the Find bar is dismissed by its presenter.
- (void)findBarDidDisappearForFindBarCoordinator:
    (FindBarCoordinator*)findBarCoordinator;

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

#endif  // IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_COORDINATOR_H_
