// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_DELEGATE_H_

@class ContextualPanelEntrypointCoordinator;

// Delegate for the ContextualPanelEntrypointCoordinator.
@protocol ContextualPanelEntrypointCoordinatorDelegate

// Whether the location bar is currently in a state where the large Contextual
// Panel entrypoint can be shown.
- (BOOL)canShowLargeContextualPanelEntrypoint:
    (ContextualPanelEntrypointCoordinator*)coordinator;

// Sets the location label of the location bar centered relative to the content
// around it when centered is passed as YES. Otherwise, resets it to the
// "absolute" center.
- (void)setLocationBarLabelCenteredBetweenContent:
            (ContextualPanelEntrypointCoordinator*)coordinator
                                         centered:(BOOL)centered;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_DELEGATE_H_
