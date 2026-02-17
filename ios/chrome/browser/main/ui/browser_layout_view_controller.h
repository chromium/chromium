// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_UI_BROWSER_LAYOUT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_MAIN_UI_BROWSER_LAYOUT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_context_provider.h"

@protocol BrowserLayoutConsumer;
@class SafeAreaProvider;

// A container view controller that manages the layout of the browser.
// It is designed to contain an instance of BrowserViewController ("BVC") as a
// child. Since the BVC itself often implements a great deal of custom logic
// around handling view controller presentation and other features, this
// containing view controller handles forwarding calls to the BVC instance where
// needed.
@interface BrowserLayoutViewController
    : UIViewController <FullscreenUIElement, TabGridTransitionContextProvider>

// The safe area provider.
@property(nonatomic, weak) SafeAreaProvider* safeAreaProvider;

// The browserViewController instance being contained. If this is set, the
// current BVC (if any) will be removed as a child view controller, and the new
// `browserViewController` will be added as a child and have its view resized to
// this object's view's bounds.
@property(nonatomic, weak)
    UIViewController<BrowserLayoutConsumer>* browserViewController;

// YES if the currentBVC is in incognito mode. Is used to set proper background
// color.
@property(nonatomic, assign) BOOL incognito;

// The container used for infobar banner overlays.
@property(nonatomic, weak)
    UIViewController* infobarBannerOverlayContainerViewController;

// The container used for infobar modal overlays.
@property(nonatomic, weak)
    UIViewController* infobarModalOverlayContainerViewController;

// The TabStripViewController instance, managed by the container's coordinator.
@property(nonatomic, weak) UIViewController* tabStripViewController;

@end

#endif  // IOS_CHROME_BROWSER_MAIN_UI_BROWSER_LAYOUT_VIEW_CONTROLLER_H_
