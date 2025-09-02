// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_CHROME_OVERLAY_WINDOW_CHROME_OVERLAY_WINDOW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_CHROME_OVERLAY_WINDOW_CHROME_OVERLAY_WINDOW_H_

#import <UIKit/UIKit.h>

// The main application window. This window hosts the application's UI and is
// also responsible for displaying overlays above the main content. It tracks
// size classes changes and reports them to SizeClassRecorder and Crash keys.
@interface ChromeOverlayWindow : UIWindow

// Activates an overlay, showing it in this window. If this is the first
// overlay, the window will be made visible.
- (void)activateOverlay:(UIView*)overlay withLevel:(UIWindowLevel)level;

// Deactivates an overlay, removing it from this window. If this is the last
// overlay, the window will be hidden.
- (void)deactivateOverlay:(UIView*)overlay;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_CHROME_OVERLAY_WINDOW_CHROME_OVERLAY_WINDOW_H_
