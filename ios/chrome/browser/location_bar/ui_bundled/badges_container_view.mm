// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/badges_container_view.h"

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_metrics.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Sets `view.hidden` to `hidden` if necessary. This helper is useful to address
// a bug where the number of times `.hidden` is set in a view accumulates if it
// is presented inside of a stack view. As a result, setting `.hidden = YES`
// twice does not have the same effect as only settings it once.
void SetViewHiddenIfNecessary(UIView* view, BOOL hidden) {
  if (view.hidden != hidden) {
    view.hidden = hidden;
  }
}

}  // namespace

@implementation LocationBarBadgesContainerView {
  UIStackView* _containerStackView;

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

#pragma mark - UIAccessibilityContainer

- (NSArray*)accessibilityElements {
  NSMutableArray* accessibleElements = [[NSMutableArray alloc] init];

  if (IsContextualPanelEnabled() && self.contextualPanelEntrypointView &&
      !self.contextualPanelEntrypointView.hidden) {
    [accessibleElements addObject:self.contextualPanelEntrypointView];
  }

  if (self.incognitoBadgeView && !self.incognitoBadgeView.hidden) {
    [accessibleElements addObject:self.incognitoBadgeView];
  }

  if (self.badgeView && !self.badgeView.hidden) {
    [accessibleElements addObject:self.badgeView];
  }

  if (self.readerModeChipView) {
    [accessibleElements addObject:self.readerModeChipView];
  }

  if (self.placeholderView && !self.placeholderView.hidden) {
    [accessibleElements addObject:self.placeholderView];
  }

  return accessibleElements;
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

#pragma mark - Setters

- (void)setIncognitoBadgeView:(UIView*)incognitoBadgeView {
  if (_incognitoBadgeView) {
    return;
  }
  _incognitoBadgeView = incognitoBadgeView;
  _incognitoBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
  _incognitoBadgeView.isAccessibilityElement = NO;
  [_containerStackView insertArrangedSubview:_incognitoBadgeView atIndex:0];
  SetViewHiddenIfNecessary(_incognitoBadgeView, YES);

  [NSLayoutConstraint activateConstraints:@[
    [_incognitoBadgeView.heightAnchor
        constraintEqualToAnchor:_containerStackView.heightAnchor],
  ]];
}

- (void)setBadgeView:(UIView*)badgeView {
  if (_badgeView) {
    return;
  }
  _badgeView = badgeView;
  _badgeView.translatesAutoresizingMaskIntoConstraints = NO;
  _badgeView.isAccessibilityElement = NO;
  [_containerStackView addArrangedSubview:_badgeView];
  SetViewHiddenIfNecessary(_badgeView, YES);

  [NSLayoutConstraint activateConstraints:@[
    [_badgeView.heightAnchor
        constraintEqualToAnchor:_containerStackView.heightAnchor],
  ]];
}

- (void)setContextualPanelEntrypointView:
    (UIView*)contextualPanelEntrypointView {
  if (IsDiamondPrototypeEnabled()) {
    return;
  }
  if (_contextualPanelEntrypointView) {
    return;
  }
  _contextualPanelEntrypointView = contextualPanelEntrypointView;
  _contextualPanelEntrypointView.translatesAutoresizingMaskIntoConstraints = NO;
  _contextualPanelEntrypointView.isAccessibilityElement = NO;
  SetViewHiddenIfNecessary(_contextualPanelEntrypointView, YES);
  // The Contextual Panel entrypoint view should be first in its containing
  // stackview, regardless of when it was added.
  [_containerStackView insertArrangedSubview:_contextualPanelEntrypointView
                                     atIndex:0];

  [NSLayoutConstraint activateConstraints:@[
    [_contextualPanelEntrypointView.heightAnchor
        constraintEqualToAnchor:_containerStackView.heightAnchor],
  ]];
}

- (void)setReaderModeChipView:(UIView*)readerModeChipView {
  if (IsDiamondPrototypeEnabled()) {
    return;
  }
  if (_readerModeChipView) {
    return;
  }
  _readerModeChipView = readerModeChipView;
  _readerModeChipView.translatesAutoresizingMaskIntoConstraints = NO;
  _readerModeChipView.isAccessibilityElement = NO;
  SetViewHiddenIfNecessary(_readerModeChipView, YES);
  // Reading Mode chip should be shown to the right of the incognito badge
  // view.
  int index = _incognitoBadgeView ? 1 : 0;
  [_containerStackView insertArrangedSubview:_readerModeChipView atIndex:index];

  [NSLayoutConstraint activateConstraints:@[
    [_readerModeChipView.heightAnchor
        constraintEqualToAnchor:_containerStackView.heightAnchor],
  ]];
}

- (void)setPlaceholderView:(UIView*)placeholderView {
  if (IsDiamondPrototypeEnabled()) {
    return;
  }
  if (_placeholderView == placeholderView) {
    return;
  }

  if ([_placeholderView superview] == _containerStackView) {
    [_placeholderView removeFromSuperview];
  }

  _placeholderView = placeholderView;
  if (_placeholderView) {
    _placeholderView.translatesAutoresizingMaskIntoConstraints = NO;
    SetViewHiddenIfNecessary(_placeholderView, YES);
    [_containerStackView addArrangedSubview:_placeholderView];
    [NSLayoutConstraint activateConstraints:@[
      [_placeholderView.heightAnchor
          constraintEqualToAnchor:_containerStackView.heightAnchor]
    ]];
  }
  [self updateViewsVisibility];
}

#pragma mark - private

// Updates the hidden state of the views.
- (void)updateViewsVisibility {
  SetViewHiddenIfNecessary(self.readerModeChipView,
                           !_readerModeChipShouldBeVisible);
  SetViewHiddenIfNecessary(self.incognitoBadgeView,
                           !_incognitoBadgeViewShouldBeVisible);
  SetViewHiddenIfNecessary(self.badgeView, !_badgeViewShouldBeVisible ||
                                               _readerModeChipShouldBeVisible);
  if (IsDiamondPrototypeEnabled()) {
    SetViewHiddenIfNecessary(self.badgeView, YES);
  }
  SetViewHiddenIfNecessary(self.contextualPanelEntrypointView,
                           !_contextualPanelEntrypointShouldBeVisible ||
                               _readerModeChipShouldBeVisible);

  BOOL placeholderHidden =
      (self.contextualPanelEntrypointView &&
       !self.contextualPanelEntrypointView.hidden) ||
      (self.badgeView && !self.badgeView.hidden) ||
      (self.readerModeChipView && !self.readerModeChipView.hidden);

  if (base::FeatureList::IsEnabled(kLensOverlayPriceInsightsCounterfactual)) {
    // Show the lens overlay entrypoint only when the price insights entrypoint
    // should have been shown.
    BOOL placeholderVisible = _contextualPanelEntrypointShouldBeVisible &&
                              (self.badgeView && self.badgeView.hidden);
    placeholderHidden = !placeholderVisible;
    if (placeholderVisible) {
      SetViewHiddenIfNecessary(self.contextualPanelEntrypointView, YES);
    }
  }

  if (!_placeholderView || placeholderHidden == _placeholderView.hidden) {
    return;
  }

  SetViewHiddenIfNecessary(_placeholderView, placeholderHidden);

  // Records why the placeholder view is hidden. These are not mutually
  // exclusive, price tracking will take precedence over messages.
  if (placeholderHidden) {
    if (self.contextualPanelEntrypointView &&
        !self.contextualPanelEntrypointView.hidden) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kPriceTracking);
    } else if (self.badgeView && !self.badgeView.hidden) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kMessage);
    } else if (self.readerModeChipView && !self.readerModeChipView.hidden) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kReaderMode);
    }
  }
}

@end
