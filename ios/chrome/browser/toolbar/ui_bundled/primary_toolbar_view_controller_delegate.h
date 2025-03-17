// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class PrimaryToolbarViewController;

// Protocol implemented by the delegate of the PrimaryToolbarViewController.
@protocol PrimaryToolbarViewControllerDelegate

// Called when the trait collection of the view controller changed.
- (void)viewControllerTraitCollectionDidChange:
    (UITraitCollection*)previousTraitCollection;

// Called when the user requires to close the toolbar (typically with the ESC/⎋
// keyboard shortcut).
- (void)close;

// Called when the location bar is expanded.
- (void)locationBarExpandedInViewController:
    (PrimaryToolbarViewController*)viewController;

// Called when the location bar is contracted.
- (void)locationBarContractedInViewController:
    (PrimaryToolbarViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_VIEW_CONTROLLER_DELEGATE_H_
