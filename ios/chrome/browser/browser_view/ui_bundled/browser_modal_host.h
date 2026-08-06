// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_MODAL_HOST_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_MODAL_HOST_H_

#import <UIKit/UIKit.h>

class Browser;

// Host for all the modal features at the Browser level. This is a class that is
// the handler for a lot of commands that only show/hide modal features.
@interface BrowserModalHost : NSObject

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// This object becomes the handler for the different command protocols it is
// hosting for.
- (void)startHostingCommandProtocols;

// The base view controller on which the modal features will be presented.
- (void)setBaseViewControllerForModals:(UIViewController*)baseViewController;

// Dismisses all currently presented modals.
- (void)clearPresentedState;

// This object stops being the handler for the different command protocols it is
// hosting for.
- (void)stopHostingCommandProtocols;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_MODAL_HOST_H_
