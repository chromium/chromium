// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_SHARE_DOWNLOAD_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SHARING_SHARE_DOWNLOAD_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

namespace web {
class WebState;
}

// Coordinator to present download overlay.
@interface ShareDownloadOverlayCoordinator : ChromeCoordinator

// Initializes a coordinator for displaying an overlay on the current displayed
// webview. `webState` must not be null.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_SHARE_DOWNLOAD_OVERLAY_COORDINATOR_H_
