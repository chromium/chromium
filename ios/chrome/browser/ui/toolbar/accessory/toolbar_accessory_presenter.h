// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ACCESSORY_TOOLBAR_ACCESSORY_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ACCESSORY_TOOLBAR_ACCESSORY_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/presenters/contained_presenter.h"

// Presenter that displays accessories over or next to the toolbar. Note that
// there are different presentations styles for iPhone (Compact Toolbar) vs.
// iPad. This is used by Find in Page.
@interface ToolbarAccessoryPresenter : NSObject <ContainedPresenter>

- (instancetype)initWithIsIncognito:(BOOL)isIncognito NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (BOOL)isPresentingViewController:(UIViewController*)viewController;

// Whether the presenter is currently presenting a view
@property(nonatomic, readonly, getter=isPresenting) BOOL presenting;

// The main presented view.
@property(nonatomic, strong, readonly) UIView* backgroundView;

// The layout guide representing the primary toolbar.
@property(nonatomic, strong) UILayoutGuide* toolbarLayoutGuide;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ACCESSORY_TOOLBAR_ACCESSORY_PRESENTER_H_
