// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_CHROME_OVERLAY_WINDOW_CHROME_OVERLAY_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_CHROME_OVERLAY_WINDOW_CHROME_OVERLAY_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

// A container view for overlays that allows touches to pass through to the
// content underneath if the touch is not on an active overlay.
@interface ChromeOverlayContainerView : UIView
@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_CHROME_OVERLAY_WINDOW_CHROME_OVERLAY_CONTAINER_VIEW_H_
