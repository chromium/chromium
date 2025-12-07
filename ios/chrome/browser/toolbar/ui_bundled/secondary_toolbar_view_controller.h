// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_SECONDARY_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_SECONDARY_TOOLBAR_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/orchestrator/ui_bundled/toolbar_animatee.h"
#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/toolbar/ui_bundled/secondary_toolbar_consumer.h"

class FullscreenController;

@protocol SecondaryToolbarKeyboardStateProvider;
@protocol ToolbarHeightDelegate;

/// View controller for the secondary part of the adaptive toolbar. It is the
/// one at the bottom of the screen. Displayed only on specific size classes.
@interface SecondaryToolbarViewController
    : AdaptiveToolbarViewController <SecondaryToolbarConsumer, ToolbarAnimatee>

/// Protocol to retrieve the keyboard state on the active web state.
@property(nonatomic, weak) id<SecondaryToolbarKeyboardStateProvider>
    keyboardStateProvider;

/// Delegate that handles the toolbars height.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;

/// Fullscreen controller used for collapsing the view above the keyboard.
@property(nonatomic, assign) FullscreenController* fullscreenController;

// Whether this toolbar is used and positioned like the primary toolbar.
// TODO(crbug.com/429955447): Remove when diamond prototype is cleaned.
@property(nonatomic, assign) BOOL usedAsPrimaryToolbar;

/// Disconnects observations and references.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_SECONDARY_TOOLBAR_VIEW_CONTROLLER_H_
