// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_H_

#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ContextualPanelEntrypointCoordinatorDelegate;
@protocol ContextualPanelEntrypointVisibilityDelegate;

// Coordinator for the Contextual Panel Entrypoint.
@interface ContextualPanelEntrypointCoordinator : ChromeCoordinator

// The delegate for this coordinator.
@property(nonatomic, weak) id<ContextualPanelEntrypointCoordinatorDelegate>
    delegate;

// The viewController visibility delegate.
@property(nonatomic, weak) id<ContextualPanelEntrypointVisibilityDelegate>
    visibilityDelegate;

// The view controller for this coordinator.
@property(nonatomic, strong)
    ContextualPanelEntrypointViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_H_
