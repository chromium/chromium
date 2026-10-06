// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_WEB_MODAL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_WEB_MODAL_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class GURL;
@class GeminiWebModalCoordinator;

// Delegate for `GeminiWebModalCoordinator`.
@protocol GeminiWebModalCoordinatorDelegate

// Called when the modal is dismissed, whether by the close button or by the
// user swiping the sheet away. The delegate should stop `coordinator`.
- (void)geminiWebModalCoordinatorDidDismiss:
    (GeminiWebModalCoordinator*)coordinator;

@end

// Presents a URL in a modal web view sheet, over the Gemini surface.
//
// The presented page runs in a detached `web::WebState` backed by the browser's
// profile and shares the profile's cookies and signed-in session.
@interface GeminiWebModalCoordinator : ChromeCoordinator

// Initializes the coordinator to present `URL`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                       URL:(const GURL&)URL
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Notified when the modal is dismissed.
@property(nonatomic, weak) id<GeminiWebModalCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_WEB_MODAL_COORDINATOR_H_
