// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace web {
struct Referrer;
}

class GURL;

// Coordinator for the link preview. Provides the preview view controller and
// handles its actions.
@interface LinkPreviewCoordinator : ChromeCoordinator

// Inits the coordinator with `browser` and the `URL` of the link.
- (instancetype)initWithBrowser:(Browser*)browser
                            URL:(const GURL&)URL NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The referrer for the preview request.
@property(nonatomic, assign) web::Referrer referrer;

// Returns the viewController for the link preview. It displays a loaded
// webState UIView.
- (UIViewController*)linkPreviewViewController;

// Handles the tap action of the preview. Called when the user taps on the
// preview of the context menu.
- (void)handlePreviewAction;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_COORDINATOR_H_
