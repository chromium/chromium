// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_PANEL_CONTENT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_PANEL_CONTENT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol TraitCollectionChangeDelegate;

// Coordinator for the contents of the Contextual Panel.
@interface PanelContentCoordinator : ChromeCoordinator

// Delegate to pass to inform when the trait collection changes for this
// feature.
@property(nonatomic, weak) id<TraitCollectionChangeDelegate>
    traitCollectionDelegate;

// Removes the panel view controller from the view hierarchy and presents it
// from the given `viewController`. Swaps from using the Contextual Panel's
// custom sheet UI to iOS's built-in UISheetController.
- (void)presentFromNewBaseViewController:(UIViewController*)viewController;

// Dismisses the panel view controller and embeds it in the given
// `viewController`. The panel view must have been presented in the past. Swaps
// from using iOS's built-in UISheetController to the Contextual Panel's custom
// sheet UI.
- (void)embedInParentViewController:(UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_PANEL_CONTENT_COORDINATOR_H_
