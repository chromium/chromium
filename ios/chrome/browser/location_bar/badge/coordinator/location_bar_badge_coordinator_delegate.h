// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_COORDINATOR_DELEGATE_H_

@class LocationBarBadgeCoordinator;

// Delegate for the LocationBarBadgeCoordinator.
@protocol LocationBarBadgeCoordinatorDelegate

// Whether the location bar is currently in a state where the large Contextual
// Panel entrypoint can be shown.
- (BOOL)canShowLargeContextualPanelEntrypoint:
    (LocationBarBadgeCoordinator*)coordinator;

// Sets the location label of the location bar centered relative to the content
// around it when centered is passed as YES. Otherwise, resets it to the
// "absolute" center.
- (void)setLocationBarLabelCenteredBetweenContent:
            (LocationBarBadgeCoordinator*)coordinator
                                         centered:(BOOL)centered;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_COORDINATOR_DELEGATE_H_
