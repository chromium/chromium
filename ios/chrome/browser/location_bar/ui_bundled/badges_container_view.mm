// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/badges_container_view.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_metrics.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_placeholder_type.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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
// The unified badge background height multiplier.
const CGFloat kBackgroundHeightMultiplier = 0.72;

// The horizontal inset for unified badge background leading and trailing edges.
const CGFloat kBackgroundHorizontalInset = 5.0;

}  // namespace

@implementation LocationBarBadgesContainerView {
  UIStackView* _containerStackView;
  UIButton* _tapOverlayButton;
  UIView* _badgeBackgroundView;
  BOOL _disableProactiveOverlay;

  /// Whether the contextual panel entrypoint should be visible. The placeholder
  /// view trumps the entrypoint when kLensOverlayPriceInsightsCounterfactual is
  /// enabled.
  BOOL _contextualPanelEntrypointShouldBeVisible;
  /// The type of the contextual panel entrypoint.
  std::optional<ContextualPanelItemType> _contextualPanelItemType;
  /// Whether the contextual panel entrypoint is currently animating.
  BOOL _contextualPanelCurrentlyAnimating;
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

  if (IsProactiveSuggestionsFrameworkEnabled() && _tapOverlayButton &&
      !_tapOverlayButton.hidden) {
    [accessibleElements addObject:_tapOverlayButton];
    return accessibleElements;
  }

  if (IsContextualPanelEnabled() && self.contextualPanelEntrypointView) {
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

- (void)setContextualPanelItemType:
    (std::optional<ContextualPanelItemType>)itemType {
  _contextualPanelItemType = itemType;
  [self updateViewsVisibility];
}

- (void)setContextualPanelCurrentlyAnimating:(BOOL)animating {
  _contextualPanelCurrentlyAnimating = animating;
  [self updateViewsVisibility];
}

- (void)disableProactiveSuggestionOverlay:(BOOL)disabled {
  _disableProactiveOverlay = disabled;
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

  if (_placeholderView && [_placeholderView superview] == _containerStackView) {
    [_containerStackView removeArrangedSubview:_placeholderView];
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

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    if (!incognito) {
      if (!_badgeBackgroundView) {
        [self setupUnifiedBadgeBackground];
      }
      _containerStackView.userInteractionEnabled = NO;
      if (!_tapOverlayButton) {
        [self setupTapOverlay];
      }
      _tapOverlayButton.hidden = NO;
    } else {
      _containerStackView.userInteractionEnabled = YES;
      if (_tapOverlayButton) {
        _tapOverlayButton.hidden = YES;
      }
      if (_badgeBackgroundView) {
        _badgeBackgroundView.hidden = YES;
      }
      self.tintColor = nil;
    }
    [self updateBackgroundVisibility];
  }
}

#pragma mark - private

// Updates the hidden state of the views.
- (void)updateViewsVisibility {
  // This is the location where the visibility of badges is decided, as a
  // function of whether each badge wants to be visible, and their types.
  BOOL contextualPanelEntrypointShouldBeVisibleFinal = NO;
  BOOL incognitoBadgeViewShouldBeVisibleFinal = NO;
  BOOL badgeViewShouldBeVisibleFinal = NO;
  BOOL readerModeChipShouldBeVisibleFinal = NO;
  BOOL placeholderViewShouldBeVisibleFinal = NO;

  // The Incognito badge should decide its visibility independently of the
  // visibility of other badges.
  incognitoBadgeViewShouldBeVisibleFinal = _incognitoBadgeViewShouldBeVisible;
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    // When framework enabled, reader mode chip visibility follows desired state
    // directly.
    readerModeChipShouldBeVisibleFinal = _readerModeChipShouldBeVisible;
  } else {
    // The Reader mode chip (which wants to be visible when Reader mode is
    // active) should not be visible if the contextual panel is currently
    // visible and animating.
    readerModeChipShouldBeVisibleFinal =
        _readerModeChipShouldBeVisible &&
        !(_contextualPanelEntrypointShouldBeVisible &&
          _contextualPanelCurrentlyAnimating);
  }

  // Other badges can be visible only outside of Reader mode.
  if (!readerModeChipShouldBeVisibleFinal) {
    // The badge view used by e.g. Translate, Permissions, etc, is visible if it
    // wants to be visible and `IsDiamondPrototypeEnabled()` returns false.
    badgeViewShouldBeVisibleFinal =
        _badgeViewShouldBeVisible && !IsDiamondPrototypeEnabled();
    // The contextual panel entrypoint can only be visible if it wants to be
    // visible and if one of these conditions is verified:
    // 1. The contextual panel has a loud moment (animating to large entrypoint)
    // OR
    // 2. The badge view (e.g. Translate) is already visible OR
    // 3. The contextual panel item is NOT the Reader Mode availability
    // contextual chip OR
    // 4. The placeholder type is NOT the page action menu placeholder.
    contextualPanelEntrypointShouldBeVisibleFinal =
        _contextualPanelEntrypointShouldBeVisible &&
        (_contextualPanelCurrentlyAnimating || badgeViewShouldBeVisibleFinal ||
         _contextualPanelItemType != ContextualPanelItemType::ReaderModeItem ||
         _placeholderType != LocationBarPlaceholderType::kPageActionMenu);
    // Finally the placeholder is visible if both the badge view and contextual
    // panel entrypoint are hidden.
    placeholderViewShouldBeVisibleFinal =
        !badgeViewShouldBeVisibleFinal &&
        !contextualPanelEntrypointShouldBeVisibleFinal;
  }

  SetViewHiddenIfNecessary(self.readerModeChipView,
                           !readerModeChipShouldBeVisibleFinal);
  SetViewHiddenIfNecessary(self.incognitoBadgeView,
                           !incognitoBadgeViewShouldBeVisibleFinal);
  SetViewHiddenIfNecessary(self.badgeView, !badgeViewShouldBeVisibleFinal);
  SetViewHiddenIfNecessary(self.contextualPanelEntrypointView,
                           !contextualPanelEntrypointShouldBeVisibleFinal);

  if (!_placeholderView ||
      !!placeholderViewShouldBeVisibleFinal == !_placeholderView.hidden) {
    return;
  }

  SetViewHiddenIfNecessary(_placeholderView,
                           !placeholderViewShouldBeVisibleFinal);

  // Records why the placeholder view is hidden. These are not mutually
  // exclusive, price tracking will take precedence over messages.
  if (!placeholderViewShouldBeVisibleFinal) {
    if (contextualPanelEntrypointShouldBeVisibleFinal) {
      if (_contextualPanelItemType) {
        switch (_contextualPanelItemType.value()) {
          case ContextualPanelItemType::PriceInsightsItem:
            RecordLensEntrypointHidden(
                IOSLocationBarLeadingIconType::kPriceTracking);
            break;
          case ContextualPanelItemType::ReaderModeItem:
            RecordLensEntrypointHidden(
                IOSLocationBarLeadingIconType::kReaderMode);
            break;
          default:
            break;
        }
      }
    } else if (badgeViewShouldBeVisibleFinal) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kMessage);
    } else if (readerModeChipShouldBeVisibleFinal) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kReaderMode);
    }
  }
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    [self updateBackgroundVisibility];
    [self updateTapOverlayButtonVisibility];
  }

  if (IsProactiveSuggestionsFrameworkEnabled() && _incognito) {
    _containerStackView.userInteractionEnabled = YES;
  }
}

// Creates and configures transparent overlay button for unified badge tapping.
- (void)setupTapOverlay {
  _tapOverlayButton = [[UIButton alloc] init];
  _tapOverlayButton.translatesAutoresizingMaskIntoConstraints = NO;
  _tapOverlayButton.backgroundColor = [UIColor clearColor];
  [_tapOverlayButton addTarget:self
                        action:@selector(handleOverlayTap:)
              forControlEvents:UIControlEventTouchUpInside];

  _tapOverlayButton.isAccessibilityElement = YES;
  _tapOverlayButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_OPEN_PAGE_ACTION_MENU);
  _tapOverlayButton.accessibilityTraits = UIAccessibilityTraitButton;

  // TODO(crbug.com/448422022): Remove overlay when migrating to
  // LocationBarBadgeViewController.
  [self addSubview:_tapOverlayButton];
  AddSameConstraints(self, _tapOverlayButton);
}

// Handles tap events on the overlay button and shows the page action menu.
- (void)handleOverlayTap:(id)sender {
  [self.pageActionMenuHandler showPageActionMenu];
}

// Creates blue background container for unified badge state.
- (void)setupUnifiedBadgeBackground {
  _badgeBackgroundView = [[UIView alloc] init];
  _badgeBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  _badgeBackgroundView.backgroundColor = [UIColor colorNamed:kBlue600Color];
  _badgeBackgroundView.userInteractionEnabled = NO;
  _badgeBackgroundView.hidden = YES;

  [self insertSubview:_badgeBackgroundView atIndex:0];

  [NSLayoutConstraint activateConstraints:@[
    [_badgeBackgroundView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kBackgroundHorizontalInset],
    [_badgeBackgroundView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kBackgroundHorizontalInset],

    [_badgeBackgroundView.heightAnchor
        constraintEqualToAnchor:self.heightAnchor
                     multiplier:kBackgroundHeightMultiplier],
    [_badgeBackgroundView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor],
  ]];
}

// Updates background visibility and badge tint colors.
- (void)updateBackgroundVisibility {
  if (!IsProactiveSuggestionsFrameworkEnabled() || !_badgeBackgroundView) {
    return;
  }

  BOOL hasVisibleBadges = [self hasVisibleBadges];

  _badgeBackgroundView.hidden = !hasVisibleBadges;

  if (hasVisibleBadges) {
    self.tintColor = [UIColor colorNamed:kSolidWhiteColor];

    _badgeBackgroundView.layer.cornerRadius =
        _badgeBackgroundView.bounds.size.height / 2.0;
  } else {
    self.tintColor = nil;
  }
}

// Returns YES if any badges are currently visible.
- (BOOL)hasVisibleBadges {
  for (UIView* subview in _containerStackView.arrangedSubviews) {
    if (!subview.hidden && subview != _placeholderView) {
      return !_disableProactiveOverlay;
    }
  }
  return NO;
}

// Updates the tap overlay button visibility.
- (void)updateTapOverlayButtonVisibility {
  _containerStackView.userInteractionEnabled = NO;

  if (_disableProactiveOverlay) {
    _tapOverlayButton.hidden = YES;
    _containerStackView.userInteractionEnabled = YES;
    return;
  }

  // If there are no visible badges, the placeholder badge should be shown and
  // we should use the default badge tap logic instead of the tap overlay.
  BOOL hasVisibleBadges = ![self hasVisibleBadges];
  _tapOverlayButton.hidden = hasVisibleBadges;
  _containerStackView.userInteractionEnabled = hasVisibleBadges;
}

@end
