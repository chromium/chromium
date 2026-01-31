// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_SECONDARY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_SECONDARY_TOOLBAR_COORDINATOR_H_

#import "ios/chrome/browser/orchestrator/ui_bundled/toolbar_animatee.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/adaptive_toolbar_coordinator.h"

// Coordinator for the secondary part of the adaptive toolbar. It is the part
// containing the controls displayed only on specific size classes.
@interface SecondaryToolbarCoordinator : AdaptiveToolbarCoordinator

// A reference to the view controller that implements the toolbar animation
// protocol.
@property(nonatomic, weak, readonly) id<ToolbarAnimatee> toolbarAnimatee;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_SECONDARY_TOOLBAR_COORDINATOR_H_
