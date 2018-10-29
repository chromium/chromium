// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

// Protocol implemented by the delegate of the PrimaryToolbarViewController.
@protocol PrimaryToolbarViewControllerDelegate

// Called when the trait collection of the view controller changed.
- (void)viewControllerTraitCollectionDidChange:
    (UITraitCollection*)previousTraitCollection;

// Exits fullscreen.
- (void)exitFullscreen;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_DELEGATE_H_
