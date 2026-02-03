// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_PRIMARY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_PRIMARY_TOOLBAR_COORDINATOR_H_

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/adaptive_toolbar_coordinator.h"

@protocol PrimaryToolbarViewControllerDelegate;
@protocol ToolbarAnimatee;

// Coordinator for the primary part, the one at the top of the screen, of the
// adaptive toolbar.
@interface PrimaryToolbarCoordinator : AdaptiveToolbarCoordinator

// A reference to the view controller that implements the tooblar animation
// protocol.
@property(nonatomic, weak, readonly) id<ToolbarAnimatee> toolbarAnimatee;
// Delegate for `primaryToolbarViewController`. Should be non-nil before start.
@property(nonatomic, weak) id<PrimaryToolbarViewControllerDelegate>
    viewControllerDelegate;
// The share button of this toolbar.
@property(nonatomic, strong, readonly) UIView* shareButton;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_PRIMARY_TOOLBAR_COORDINATOR_H_
