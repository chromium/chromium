// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_DELEGATE_H_

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_tab_change_audience.h"

class GURL;
@class LensOverlayMediator;

/// Delegate for events in `LensOverlayMediator`.
@protocol LensOverlayMediatorDelegate <NSObject>

/// The lens overlay menu (3-dots) did open.
- (void)lensOverlayMediatorDidOpenOverlayMenu:(LensOverlayMediator*)mediator;

/// Called when an URL needs to be opened in a new tab.
- (void)lensOverlayMediatorOpenURLInNewTabRequested:(GURL)url;

/// Called when a translation failed due to no or unprocessable text in the
/// given image.
- (void)lensOverlayMediatorDidFailDetectingTranslatableText;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_DELEGATE_H_
