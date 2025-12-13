// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_ACCESSORY_TOOLBAR_ACCESSORY_PRESENTER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_ACCESSORY_TOOLBAR_ACCESSORY_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/presenters/ui_bundled/contained_presenter.h"

class OmniboxPositionBrowserAgent;

/// Presenter that displays accessories over or next to the toolbar. Note that
/// there are different presentations styles for iPhone (Compact Toolbar) vs.
/// iPad.
@interface ToolbarAccessoryPresenter : NSObject <ContainedPresenter>

/// Whether the presenter is currently presenting a view.
@property(nonatomic, readonly, getter=isPresenting) BOOL presenting;

/// The main presented view.
@property(nonatomic, strong, readonly) UIView* backgroundView;

/// The layout guide representing the top toolbar.
@property(nonatomic, strong) UILayoutGuide* topToolbarLayoutGuide;

/// The layout guide representing the bottom toolbar.
@property(nonatomic, strong) UILayoutGuide* bottomToolbarLayoutGuide;

- (instancetype)initWithIsIncognito:(BOOL)isIncognito
        omniboxPositionBrowserAgent:
            (OmniboxPositionBrowserAgent*)omniboxPositionBrowserAgent
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/// Removes C++ object reference.
- (void)disconnect;

- (BOOL)isPresentingViewController:(UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_ACCESSORY_TOOLBAR_ACCESSORY_PRESENTER_H_
