// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

@class LocationBarBadgeMediator;

// TODO(crbug.com/454351425): Refactor function names to not use "entrypoint".
// Usage is for parity with ContextualPanelEntryPointConsumer.
// Delegate for the LocationBarBadgeMediator;
@protocol LocationBarBadgeMediatorDelegate

// Whether the location bar is currently in a state where the large Contextual
// Panel entrypoint can be shown.
- (BOOL)canShowLargeContextualPanelEntrypoint:
    (LocationBarBadgeMediator*)mediator;

// Sets the location label of the location bar centered relative to the content
// around it when centered is passed as YES. Otherwise, resets it to the
// "absolute" center.
- (void)setLocationBarLabelCenteredBetweenContent:
            (LocationBarBadgeMediator*)mediator
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

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_DELEGATE_H_
