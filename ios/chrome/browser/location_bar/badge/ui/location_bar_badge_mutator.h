// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_MUTATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_MUTATOR_H_

@class LocationBarBadgeConfiguration;
enum class LocationBarBadgeType;

// TODO(crbug.com/454351425): Refactor function names to not use "entrypoint".
// Usage is for parity with ContextualPanelEntryPointConsumer.
// Mutator for LocationBarBadgeViewController.
@protocol LocationBarBadgeMutator

// Notify the mutator to dismiss the entrypoint's IPH.
- (void)dismissIPHAnimated:(BOOL)animated;

// Notify the mutator that the badge was tapped.
- (void)badgeTapped:(LocationBarBadgeConfiguration*)badgeConfig;

// Sets the location label of the location bar centered relative to the
// content around it when centered is passed as YES. Otherwise, resets it to
// the "absolute" center.
- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered;

// Handles collapse of badge container. Can be used to handle FET dismissal.
// Only called if the badge container was expanded and finished collapsing.
- (void)handleBadgeContainerCollapse:(LocationBarBadgeType)badgeType;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_MUTATOR_H_
