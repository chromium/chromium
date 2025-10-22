// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_view_controller.h"

#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_mutator.h"
#import "ios/chrome/browser/location_bar/badge/ui/badge_type.h"
#import "ios/chrome/browser/location_bar/ui_bundled/badges_container_view.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_metrics.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation LocationBarBadgeViewController {
  /// Whether the contextual panel entrypoint should be visible. The placeholder
  /// view trumps the entrypoint when kLensOverlayPriceInsightsCounterfactual is
  /// enabled.
  BOOL _contextualPanelEntrypointShouldBeVisible;
  /// Whether the incognito badge view should be visible.
  BOOL _incognitoBadgeViewShouldBeVisible;
  /// Whether the badge view should be visible.
  BOOL _badgeViewShouldBeVisible;
  /// Whether the reader mode chip should be visible.
  BOOL _readerModeChipShouldBeVisible;
}

#pragma mark - Public

// TODO(crbug.com/450006763): Implement this properly.
- (void)viewDidLoad {
  self.view.isAccessibilityElement = NO;
  self.view.hidden = YES;
  [NSLayoutConstraint activateConstraints:@[
    [self.view.heightAnchor constraintEqualToAnchor:super.view.heightAnchor],
  ]];
}

#pragma mark - LocationBarBadgeConsumer

- (void)setBadge:(LocationBarBadgeType)badge hidden:(BOOL)hidden {
  switch (badge) {
    case LocationBarBadgeType::kNone:
      break;
    case LocationBarBadgeType::kBadgeView:
      [self setBadgeViewHidden:hidden];
      break;
    case LocationBarBadgeType::kIncognito:
      [self setIncognitoBadgeViewHidden:hidden];
      break;
    case LocationBarBadgeType::kContextualPanel:
      [self setContextualPanelEntrypointHidden:hidden];
      break;
    case LocationBarBadgeType::kReaderMode:
      // Reader chip coordinator isn't needed for setting visibility.
      [self readerModeChipCoordinator:nil didSetReaderModeChipHidden:hidden];
      break;
  }
}

#pragma mark - IncognitoBadgeViewVisibilityDelegate

- (void)setIncognitoBadgeViewHidden:(BOOL)hidden {
  _incognitoBadgeViewShouldBeVisible = !hidden;
  [self updateViewsVisibility];
}

#pragma mark - BadgeViewVisibilityDelegate

- (void)setBadgeViewHidden:(BOOL)hidden {
  _badgeViewShouldBeVisible = !hidden;
  [self updateViewsVisibility];
}

#pragma mark - ContextualPanelEntrypointVisibilityDelegate

- (void)setContextualPanelEntrypointHidden:(BOOL)hidden {
  _contextualPanelEntrypointShouldBeVisible = !hidden;
  [self updateViewsVisibility];
}

#pragma mark - ReaderModeChipVisibilityDelegate

- (void)readerModeChipCoordinator:(ReaderModeChipCoordinator*)coordinator
       didSetReaderModeChipHidden:(BOOL)hidden {
  _readerModeChipShouldBeVisible = !hidden;
  [self updateViewsVisibility];
}

#pragma mark - Private

// Updates the hidden state of the views.
- (void)updateViewsVisibility {
  // TODO(crbug.com/450006763): Based on which view should be visible,
  // manipulate self.view to change to a specific badge or chip. This replaces
  // SetViewHiddenIfNecessary() in LocationBarBadgesContainerView.

  // Whether the default/placeholder badge should show. Only shown if no other
  // badge or chip is shown.
  BOOL placeholderHidden =
      self.view && !_contextualPanelEntrypointShouldBeVisible &&
      !_badgeViewShouldBeVisible && !_readerModeChipShouldBeVisible;

  if (!self.view || placeholderHidden == self.view.hidden) {
    return;
  }

  // TODO(crbug.com/450006763): If no priority badge shows, manipulate self.view
  // to change into "placeholder" badge.

  // Records why the placeholder view is hidden. These are not mutually
  // exclusive, price tracking will take precedence over messages.
  if (placeholderHidden) {
    if (_contextualPanelEntrypointShouldBeVisible) {
      // TODO(crbug.com/454072799): Adapt to record hiding badges for any badge
      // that goes through LocationBarBadge.
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kPriceTracking);
    } else if (_badgeViewShouldBeVisible) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kMessage);
    } else if (_readerModeChipShouldBeVisible) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kReaderMode);
    }
  }
}

- (CGPoint)helpAnchorUsingBottomOmnibox:(BOOL)isBottomOmnibox {
  // TODO (crbug.com/450006763): Implement this method.
  return CGPointMake(0, 0);
}

#pragma mark - ContextualPanelEntrypointConsumer

- (void)setEntrypointConfig:(ContextualPanelItemConfiguration*)config {
  // TODO (crbug.com/450006763): Implement this method.
}

- (void)setInfobarBadgesCurrentlyShown:(BOOL)infobarBadgesCurrentlyShown {
  // TODO (crbug.com/450006763): Implement this method.
}

- (void)hideEntrypoint {
  // TODO (crbug.com/450006763): Implement this method.
}

- (void)showEntrypoint {
  // TODO (crbug.com/450006763): Implement this method.
}

- (void)transitionToLargeEntrypoint {
  // TODO (crbug.com/450006763): Implement this method.
}

- (void)transitionToSmallEntrypoint {
  // TODO (crbug.com/450006763): Implement this method.
}

- (void)transitionToContextualPanelOpenedState:(BOOL)opened {
  // TODO (crbug.com/450006763): Implement this method.
}

- (void)setEntrypointColored:(BOOL)colored {
  // TODO (crbug.com/450006763): Implement this method.
}

#pragma mark FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  // TODO (crbug.com/450006763): Implement this method.
}

@end
