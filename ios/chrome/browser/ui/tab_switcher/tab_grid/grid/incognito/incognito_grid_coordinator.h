// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator_delegate.h"

@protocol GridMediatorDelegate;
@protocol GridToolbarsMutator;
@class IncognitoGridMediator;
@class TabGridViewController;

@interface IncognitoGridCoordinator
    : ChromeCoordinator <IncognitoGridMediatorDelegate>
// Incognito view controller.
// TODO(crbug.com/1457146): Replace this once the incognito grid view controller
// is created.
@property(nonatomic, weak) TabGridViewController* incognitoViewController;
// Incognito grid mediator.
@property(nonatomic, readonly, weak)
    IncognitoGridMediator* incognitoGridMediator;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           toolbarsMutator:
                               (id<GridToolbarsMutator>)toolbarsMutator
                      gridMediatorDelegate:(id<GridMediatorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_COORDINATOR_H_
