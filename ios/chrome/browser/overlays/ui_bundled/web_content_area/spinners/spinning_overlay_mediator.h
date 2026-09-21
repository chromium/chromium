// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_WEB_CONTENT_AREA_SPINNERS_SPINNING_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_WEB_CONTENT_AREA_SPINNERS_SPINNING_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/spinners/spinning_overlay_view_delegate.h"

// Mediator for the spinning overlay.
@interface SpinningOverlayMediator
    : OverlayRequestMediator <SpinningOverlayViewDelegate>

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_WEB_CONTENT_AREA_SPINNERS_SPINNING_OVERLAY_MEDIATOR_H_
