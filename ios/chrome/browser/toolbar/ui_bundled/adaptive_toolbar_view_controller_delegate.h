// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_DELEGATE_H_

enum class FullscreenExitReason;

// Protocol implemented by the delegate of the AdaptiveToolbarViewController.
@protocol AdaptiveToolbarViewControllerDelegate

// Exits fullscreen.
- (void)exitFullscreen:(FullscreenExitReason)FullscreenExitReason;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_DELEGATE_H_
