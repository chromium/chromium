// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/badges_container_view.h"

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_metrics.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation LocationBarBadgesContainerView {
  UIStackView* _containerStackView;

  /// Whether the contextual panel entrypoint should be visible. The placeholder
  /// view trumps the entrypoint when kLensOverlayPriceInsightsCounterfactual is
  /// enabled.
  BOOL _contextualPanelEntrypointShouldBeVisible;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];

  if (self) {
    _containerStackView = [[UIStackView alloc] init];
    _containerStackView.isAccessibilityElement = NO;
    _containerStackView.axis = UILayoutConstraintAxisHorizontal;
    _containerStackView.alignment = UIStackViewAlignmentCenter;
    _containerStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_containerStackView];
    AddSameConstraints(self, _containerStackView);
  }

  return self;
}

- (NSArray*)accessibilityElements {
  NSMutableArray* accessibleElements = [[NSMutableArray alloc] init];

  if (IsContextualPanelEnabled() && self.contextualPanelEntrypointView &&
      !self.contextualPanelEntrypointView.hidden) {
    [accessibleElements addObject:self.contextualPanelEntrypointView];
  }

  if (self.badgeView && !self.badgeView.hidden) {
    [accessibleElements addObject:self.badgeView];
  }

  if (self.placeholderView && !self.placeholderView.hidden) {
    [accessibleElements addObject:self.placeholderView];
  }

  return accessibleElements;
}

#pragma mark - BadgeViewVisibilityDelegate

- (void)setBadgeViewHidden:(BOOL)hidden {
  _badgeView.hidden = hidden;
  [self updatePlaceholderVisibility];
}

#pragma mark - ContextualPanelEntrypointVisibilityDelegate

- (void)setContextualPanelEntrypointHidden:(BOOL)hidden {
  _contextualPanelEntrypointView.hidden = hidden;
  _contextualPanelEntrypointShouldBeVisible = !hidden;
  [self updatePlaceholderVisibility];
}

#pragma mark - Setters

- (void)setBadgeView:(UIView*)badgeView {
  if (_badgeView) {
    return;
  }
  _badgeView = badgeView;
  _badgeView.translatesAutoresizingMaskIntoConstraints = NO;
  _badgeView.isAccessibilityElement = NO;
  [_containerStackView addArrangedSubview:_badgeView];
  _badgeView.hidden = YES;

  [NSLayoutConstraint activateConstraints:@[
    [_badgeView.heightAnchor
        constraintEqualToAnchor:_containerStackView.heightAnchor],
  ]];
}

- (void)setContextualPanelEntrypointView:
    (UIView*)contextualPanelEntrypointView {
  if (_contextualPanelEntrypointView) {
    return;
  }
  _contextualPanelEntrypointView = contextualPanelEntrypointView;
  _contextualPanelEntrypointView.translatesAutoresizingMaskIntoConstraints = NO;
  _contextualPanelEntrypointView.isAccessibilityElement = NO;
  _contextualPanelEntrypointView.hidden = YES;
  // The Contextual Panel entrypoint view should be first in its containing
  // stackview, regardless of when it was added.
  [_containerStackView insertArrangedSubview:_contextualPanelEntrypointView
                                     atIndex:0];

  [NSLayoutConstraint activateConstraints:@[
    [_contextualPanelEntrypointView.heightAnchor
        constraintEqualToAnchor:_containerStackView.heightAnchor],
  ]];
}

- (void)setPlaceholderView:(UIView*)placeholderView {
  if (_placeholderView == placeholderView) {
    return;
  }

  if ([_placeholderView superview] == _containerStackView) {
    [_placeholderView removeFromSuperview];
  }

  _placeholderView = placeholderView;
  if (_placeholderView) {
    _placeholderView.translatesAutoresizingMaskIntoConstraints = NO;
    _placeholderView.hidden = YES;
    [_containerStackView addArrangedSubview:_placeholderView];
    [NSLayoutConstraint activateConstraints:@[
      [_placeholderView.heightAnchor
          constraintEqualToAnchor:_containerStackView.heightAnchor]
    ]];
  }
  [self updatePlaceholderVisibility];
}

#pragma mark - private

// Updates the hidden state of the placeholder view.
- (void)updatePlaceholderVisibility {
  BOOL placeholderHidden = (self.contextualPanelEntrypointView &&
                            !self.contextualPanelEntrypointView.hidden) ||
                           (self.badgeView && !self.badgeView.hidden);

  if (base::FeatureList::IsEnabled(kLensOverlayPriceInsightsCounterfactual)) {
    // Show the lens overlay entrypoint only when the price insights entrypoint
    // should have been shown.
    BOOL placeholderVisible = _contextualPanelEntrypointShouldBeVisible &&
                              (!self.badgeView || self.badgeView.hidden);
    placeholderHidden = !placeholderVisible;
    if (placeholderVisible) {
      self.contextualPanelEntrypointView.hidden = YES;
    }
  }

  if (!_placeholderView || placeholderHidden == _placeholderView.hidden) {
    return;
  }

  _placeholderView.hidden = placeholderHidden;

  // Records why the placeholder view is hidden. These are not mutually
  // exclusive, price tracking will take precedence over messages.
  if (placeholderHidden) {
    if (self.contextualPanelEntrypointView &&
        !self.contextualPanelEntrypointView.hidden) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kPriceTracking);
    } else if (self.badgeView && !self.badgeView.hidden) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kMessage);
    }
  }
}

@end
