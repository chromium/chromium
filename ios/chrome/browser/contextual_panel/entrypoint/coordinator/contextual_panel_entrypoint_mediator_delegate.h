// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_MEDIATOR_DELEGATE_H_

@class ContextualPanelEntrypointMediator;

// Delegate for the ContextualPanelEntrypointMediator;
@protocol ContextualPanelEntrypointMediatorDelegate

// Whether the location bar is currently in a state where the large Contextual
// Panel entrypoint can be shown.
- (BOOL)canShowLargeContextualPanelEntrypoint:
    (ContextualPanelEntrypointMediator*)mediator;

// Sets the location label of the location bar centered relative to the content
// around it when centered is passed as YES. Otherwise, resets it to the
// "absolute" center.
- (void)setLocationBarLabelCenteredBetweenContent:
            (ContextualPanelEntrypointMediator*)mediator
                                         centered:(BOOL)centered;

// Disables fullscreen until `enableFullscreen` is called afterwards.
- (void)disableFullscreen;

// Re-enables fullscreen, no-op if fullscreen was not disabled beforehand.
- (void)enableFullscreen;

// Gets the current bottom omnibox state.
- (BOOL)isBottomOmniboxActive;

// Gets the in-product help anchor point in window coordinates.
- (CGPoint)helpAnchorUsingBottomOmnibox:(BOOL)isBottomOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_MEDIATOR_DELEGATE_H_
