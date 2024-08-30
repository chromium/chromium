// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/badges_container_view.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation LocationBarBadgesContainerView {
  UIStackView* _containerStackView;
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

- (NSMutableArray*)accessibleElements {
  NSMutableArray* accessibleElements = [[NSMutableArray alloc] init];

  if (IsContextualPanelEnabled() && self.contextualPanelEntrypointView &&
      !self.contextualPanelEntrypointView.hidden) {
    [accessibleElements addObject:self.contextualPanelEntrypointView];
  }

  if (self.badgeView && !self.badgeView.hidden) {
    [accessibleElements addObject:self.badgeView];
  }

  return accessibleElements;
}

#pragma mark - BadgeViewVisibilityDelegate

- (void)setBadgeViewHidden:(BOOL)hidden {
  _badgeView.hidden = hidden;
}

#pragma mark - ContextualPanelEntrypointVisibilityDelegate

- (void)setContextualPanelEntrypointHidden:(BOOL)hidden {
  _contextualPanelEntrypointView.hidden = hidden;
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
  // The Contextual Panel entrypoint view should be first in its containing
  // stackview, regardless of when it was added.
  [_containerStackView insertArrangedSubview:_contextualPanelEntrypointView
                                     atIndex:0];

  [NSLayoutConstraint activateConstraints:@[
    [_contextualPanelEntrypointView.heightAnchor
        constraintEqualToAnchor:_containerStackView.heightAnchor],
  ]];
}

@end
