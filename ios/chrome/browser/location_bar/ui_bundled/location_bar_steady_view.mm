// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view.h"

#import "base/check.h"
#import "base/check_op.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_view_visibility_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_visibility_delegate.h"
#import "ios/chrome/browser/location_bar/ui_bundled/badges_container_view.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Length of the trailing button side.
const CGFloat kButtonSize = 24;
// Space between the location icon and the location label.
const CGFloat kLocationImageToLabelSpacing = -2.0;
// Minimal horizontal padding between the leading edge of the location bar and
// the content of the location bar.
const CGFloat kLocationBarLeadingPadding = 8.0;
// Trailing space between the trailing button and the trailing edge of the
// location bar.
const CGFloat kShareButtonTrailingSpacing = -11;
const CGFloat kVoiceSearchButtonTrailingSpacing = -7;
// Location label vertical offset.
const CGFloat kLocationLabelVerticalOffset = -1;
// The margin from the leading side when not centered.
const CGFloat kLeadingMargin = 20;
// The multiplier for the smaller location label font, used when animating in
// the large Contextual Panel entrypoint.
const CGFloat kSmallerLocationLabelFontMultiplier = 0.75;
}  // namespace

@interface LocationBarSteadyView ()

// The image view displaying the current location icon (i.e. http[s] status).
@property(nonatomic, strong) UIImageView* locationIconImageView;

// The view containing the location label, and (sometimes) the location image
// view.
@property(nonatomic, strong) UIView* locationContainerView;

// Leading constraint for locationContainerView when there is no BadgeView to
// its left.
@property(nonatomic, strong)
    NSLayoutConstraint* locationContainerViewLeadingAnchorConstraint;

// The constraint that pins the trailingButton to the trailing edge of the
// location bar.
@property(nonatomic, strong)
    NSLayoutConstraint* trailingButtonTrailingAnchorConstraint;

// The trailing spacing to be used for the trailingButton. This property is
// based on the type of trailing button in use (i.e. share or voice search).
@property(nonatomic, readonly) CGFloat trailingButtonTrailingSpacing;

// Constraints to pin the badges container stackview to the right next to the
// `locationContainerView`.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* badgesViewFullScreenEnabledConstraints;

// Constraints to pin the badges container stackview to the left side of the
// LocationBar.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* badgesViewFullScreenDisabledConstraints;

// Constraints to hide the location image view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* hideLocationImageConstraints;

// Constraints to show the location image view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* showLocationImageConstraints;

// Elements to surface in accessibility.
@property(nonatomic, strong) NSMutableArray* accessibleElements;

@end

#pragma mark - LocationBarSteadyViewColorScheme

@implementation LocationBarSteadyViewColorScheme

+ (instancetype)standardScheme {
  LocationBarSteadyViewColorScheme* scheme =
      [[LocationBarSteadyViewColorScheme alloc] init];

  scheme.fontColor = [UIColor colorNamed:kTextPrimaryColor];
  scheme.placeholderColor = content_suggestions::SearchHintLabelColor();
  scheme.trailingButtonColor = [UIColor colorNamed:kGrey600Color];

  return scheme;
}

@end

#pragma mark - LocationBarSteadyButton

// Buttons with a darker background in highlighted state.
@interface LocationBarSteadyButton : UIButton
@end

@implementation LocationBarSteadyButton

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.pointerInteractionEnabled = YES;
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius = self.bounds.size.height / 2.0;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  CGFloat duration = highlighted ? 0.1 : 0.2;
  [UIView animateWithDuration:duration
                        delay:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     CGFloat alpha = highlighted ? 0.07 : 0;
                     self.backgroundColor =
                         [UIColor colorWithWhite:0 alpha:alpha];
                   }
                   completion:nil];
}

@end

#pragma mark - LocationBarSteadyView

@implementation LocationBarSteadyView {
  // The different X anchor constraints that can apply to the location label at
  // a given time.
  NSLayoutConstraint* _xStickToLeadingSideConstraint;
  NSLayoutConstraint* _xAbsoluteCenteredConstraint;
  NSLayoutConstraint* _xRelativeToContentCenteredConstraint;

  // LayoutGuide centered between the contents at the edges of the location bar.
  // (i.e. the layout guide will push towards the trailing side when the
  // entrypoint is present on the leading edge.)
  UILayoutGuide* _centeredBetweenLocationBarContentsLayoutGuide;

  // The trailing view that is hidden by default, shown for highlight mode.
  UIView* _trailingButtonSpotlightView;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self setUpViews];
    [self setUpLayout];
    [self setUpTraitChangeHandler];
  }
  [self setUpAccessibility];
  return self;
}

- (void)setUpViews {
  _locationLabel = [[UILabel alloc] init];
  _locationIconImageView = [[UIImageView alloc] init];
  _locationIconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [_locationIconImageView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  SetA11yLabelAndUiAutomationName(
      _locationIconImageView,
      IDS_IOS_PAGE_INFO_SECURITY_BUTTON_ACCESSIBILITY_LABEL,
      @"Page Security Info");
  _locationIconImageView.isAccessibilityElement = YES;

  // Setup trailing button.
  _trailingButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  _trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
  _trailingButton.pointerInteractionEnabled = YES;
  // Make the pointer shape fit the location bar's semi-circle end shape.
  _trailingButton.pointerStyleProvider =
      CreateLiftEffectCirclePointerStyleProvider();

  __weak __typeof(self) weakSelf = self;
  CustomHighlightableButtonHighlightHandler handler = ^(BOOL highlighted) {
    [weakSelf updateTrailingButtonWithHighlightedStatus:highlighted];
  };
  [_trailingButton setCustomHighlightHandler:handler];

  // Setup label.
  _locationLabel.lineBreakMode = NSLineBreakByTruncatingHead;
  _locationLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [_locationLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  _locationLabel.font = [self locationLabelFont];

  // Container for location label and icon.
  _locationContainerView = [[UIView alloc] init];
  _locationContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _locationContainerView.userInteractionEnabled = NO;
  [_locationContainerView addSubview:_locationIconImageView];
  [_locationContainerView addSubview:_locationLabel];

  _trailingButtonSpotlightView = [[UIView alloc] init];
  _trailingButtonSpotlightView.translatesAutoresizingMaskIntoConstraints = NO;
  _trailingButtonSpotlightView.hidden = YES;
  _trailingButtonSpotlightView.userInteractionEnabled = NO;
  _trailingButtonSpotlightView.backgroundColor =
      [UIColor colorNamed:kBlueColor];

  _locationButton = [[LocationBarSteadyButton alloc] init];
  _locationButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_locationButton addSubview:_trailingButton];
  [_locationButton insertSubview:_trailingButtonSpotlightView
                    belowSubview:_trailingButton];
  [_locationButton addSubview:_locationContainerView];
  AddSameCenterConstraints(_trailingButton, _trailingButtonSpotlightView);

  [self addSubview:_locationButton];

  AddSameConstraints(self, _locationButton);

  // Badges (infobar badge , Contextual Panel & Lens Overlay entypoints)
  // container view.
  _badgesContainerView = [[LocationBarBadgesContainerView alloc] init];
  _badgesContainerView.translatesAutoresizingMaskIntoConstraints = NO;

  [_locationButton addSubview:_badgesContainerView];
}

- (void)setUpLayout {
  _showLocationImageConstraints = @[
    [_locationContainerView.leadingAnchor
        constraintEqualToAnchor:_locationIconImageView.leadingAnchor],
    [_locationIconImageView.trailingAnchor
        constraintEqualToAnchor:_locationLabel.leadingAnchor
                       constant:kLocationImageToLabelSpacing],
    [_locationLabel.trailingAnchor
        constraintEqualToAnchor:_locationContainerView.trailingAnchor],
    [_locationIconImageView.centerYAnchor
        constraintEqualToAnchor:_locationContainerView.centerYAnchor],
  ];

  _hideLocationImageConstraints = @[
    [_locationContainerView.leadingAnchor
        constraintEqualToAnchor:_locationLabel.leadingAnchor],
    [_locationLabel.trailingAnchor
        constraintEqualToAnchor:_locationContainerView.trailingAnchor],
  ];

  [NSLayoutConstraint activateConstraints:_showLocationImageConstraints];

  self.badgesViewFullScreenEnabledConstraints = @[
    [_badgesContainerView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.leadingAnchor],
    [_badgesContainerView.trailingAnchor
        constraintEqualToAnchor:self.locationContainerView.leadingAnchor],
  ];

  self.badgesViewFullScreenDisabledConstraints = @[
    [_badgesContainerView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor],
    [_badgesContainerView.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.locationContainerView
                                              .leadingAnchor],
  ];

  // This low-priority, 0 width constraint is necessary for the stackview to
  // return to its 0 size when empty and exiting fullscreen.
  NSLayoutConstraint* badgesContainerViewWidthConstraint =
      [_badgesContainerView.widthAnchor constraintEqualToConstant:0];
  badgesContainerViewWidthConstraint.priority = UILayoutPriorityDefaultLow - 1;

  [NSLayoutConstraint
      activateConstraints:[self.badgesViewFullScreenDisabledConstraints
                              arrayByAddingObjectsFromArray:@[
                                [_badgesContainerView.topAnchor
                                    constraintEqualToAnchor:self.topAnchor],
                                [_badgesContainerView.bottomAnchor
                                    constraintEqualToAnchor:self.bottomAnchor],
                                badgesContainerViewWidthConstraint,
                              ]]];

  // Different possible X anchors for the location label container.
  _xStickToLeadingSideConstraint = [_locationContainerView.leadingAnchor
      constraintEqualToAnchor:self.leadingAnchor
                     constant:kLeadingMargin];
  _xStickToLeadingSideConstraint.priority = UILayoutPriorityDefaultHigh;

  _xAbsoluteCenteredConstraint = [_locationContainerView.centerXAnchor
      constraintEqualToAnchor:self.centerXAnchor];
  _xAbsoluteCenteredConstraint.priority = UILayoutPriorityDefaultHigh;

  _locationContainerViewLeadingAnchorConstraint =
      [_locationContainerView.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                      constant:kLocationBarLeadingPadding];

  if (IsContextualPanelEnabled()) {
    // Setup the layout guide centered between the contents of the location
    // bar.
    _centeredBetweenLocationBarContentsLayoutGuide =
        [[UILayoutGuide alloc] init];
    [_locationButton
        addLayoutGuide:_centeredBetweenLocationBarContentsLayoutGuide];
    [NSLayoutConstraint activateConstraints:@[
      [_centeredBetweenLocationBarContentsLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_badgesContainerView.trailingAnchor],
      [_centeredBetweenLocationBarContentsLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_trailingButton.leadingAnchor],
    ]];

    _xRelativeToContentCenteredConstraint = [_locationContainerView
                                                 .centerXAnchor
        constraintEqualToAnchor:_centeredBetweenLocationBarContentsLayoutGuide
                                    .centerXAnchor];
    _xRelativeToContentCenteredConstraint.priority =
        UILayoutPriorityDefaultHigh - 1;
  }

  _trailingButtonTrailingAnchorConstraint = [self.trailingButton.trailingAnchor
      constraintEqualToAnchor:self.trailingAnchor
                     constant:self.trailingButtonTrailingSpacing];

  // Setup and activate constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_locationLabel.centerYAnchor
        constraintEqualToAnchor:_locationContainerView.centerYAnchor
                       constant:kLocationLabelVerticalOffset],
    [_locationLabel.heightAnchor
        constraintLessThanOrEqualToAnchor:_locationContainerView.heightAnchor
                                 constant:2 * kLocationLabelVerticalOffset],
    [_trailingButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_locationContainerView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor],
    [_trailingButton.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_locationContainerView
                                                 .trailingAnchor],
    [_trailingButton.widthAnchor constraintEqualToConstant:kButtonSize],
    [_trailingButton.heightAnchor constraintEqualToConstant:kButtonSize],
    _trailingButtonTrailingAnchorConstraint,
    _xAbsoluteCenteredConstraint,
    _locationContainerViewLeadingAnchorConstraint,
    [_trailingButtonSpotlightView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [_trailingButtonSpotlightView.heightAnchor
        constraintEqualToAnchor:self.heightAnchor],
  ]];
}

- (void)setUpTraitChangeHandler {
  if (@available(iOS 17, *)) {
    __weak __typeof(self) weakSelf = self;
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.self ]);
    UITraitChangeHandler traitChangeHandler =
        ^(id<UITraitEnvironment> traitEnvironment,
          UITraitCollection* previousCollection) {
          [weakSelf updateFontOnTraitChange:previousCollection];
        };
    [self registerForTraitChanges:traits withHandler:traitChangeHandler];
  }
}

- (void)setUpAccessibility {
  // Setup accessibility.
  _trailingButton.isAccessibilityElement = YES;
  _locationButton.isAccessibilityElement = YES;
  _locationButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_LOCATION);

  _accessibleElements = [[NSMutableArray alloc] init];
  [_accessibleElements addObject:_locationButton];
  [_accessibleElements addObject:_trailingButton];

  // These two elements must remain accessible for egtests, but will not be
  // included in accessibility navigation as they are not added to the
  // accessibleElements array.
  _locationIconImageView.isAccessibilityElement = YES;
  _locationLabel.isAccessibilityElement = YES;

  [self updateAccessibility];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  _trailingButtonSpotlightView.layer.cornerRadius =
      _trailingButtonSpotlightView.bounds.size.height / 2;
}

- (CGFloat)trailingButtonTrailingSpacing {
  if (IsSplitToolbarMode(self)) {
    return kShareButtonTrailingSpacing;
  } else {
    return kVoiceSearchButtonTrailingSpacing;
  }
}

- (void)setColorScheme:(LocationBarSteadyViewColorScheme*)colorScheme {
  _colorScheme = colorScheme;
  self.trailingButton.tintColor = self.colorScheme.trailingButtonColor;
  // The text color is set in -setLocationLabelText: and
  // -setLocationLabelPlaceholderText: because the two text styles have
  // different colors. The icon should be the same color as the text, but it
  // only appears with the regular label, so its color can be set here.
  self.locationIconImageView.tintColor = self.colorScheme.fontColor;
}

- (void)setLocationImage:(UIImage*)locationImage {
  BOOL hadImage = self.locationIconImageView.image != nil;
  BOOL hasImage = locationImage != nil;
  self.locationIconImageView.image = locationImage;
  if (hadImage == hasImage) {
    return;
  }

  if (hasImage) {
    [self.locationContainerView addSubview:self.locationIconImageView];
    [NSLayoutConstraint
        deactivateConstraints:self.hideLocationImageConstraints];
    [NSLayoutConstraint activateConstraints:self.showLocationImageConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:self.showLocationImageConstraints];
    [NSLayoutConstraint activateConstraints:self.hideLocationImageConstraints];
    [self.locationIconImageView removeFromSuperview];
  }
}

- (void)setLocationLabelText:(NSString*)string {
  if ([self.locationLabel.text isEqualToString:string]) {
    return;
  }
  self.locationLabel.textColor = self.colorScheme.fontColor;
  self.locationLabel.text = string;
  [self updateAccessibility];
}

- (void)setLocationLabelPlaceholderText:(NSString*)string {
  self.locationLabel.textColor = self.colorScheme.placeholderColor;
  self.locationLabel.text = string;
}

- (void)setSecurityLevelAccessibilityString:(NSString*)string {
  if ([_securityLevelAccessibilityString isEqualToString:string]) {
    return;
  }
  _securityLevelAccessibilityString = [string copy];
  [self updateAccessibility];
}

- (void)setBadgeView:(UIView*)badgeView {
  BOOL hadBadgeView = _badgesContainerView.badgeView != nil;
  if (!hadBadgeView && badgeView) {
    _badgesContainerView.badgeView = badgeView;
  }
  [self updateAccessibility];
}

- (void)setContextualPanelEntrypointView:
    (UIView*)contextualPanelEntrypointView {
  BOOL hadEntrypointView =
      _badgesContainerView.contextualPanelEntrypointView != nil;
  if (!hadEntrypointView && contextualPanelEntrypointView) {
    _badgesContainerView.contextualPanelEntrypointView =
        contextualPanelEntrypointView;
  }
  [self updateAccessibility];
}

- (void)setPlaceholderView:(UIView*)placeholderView {
  if (_badgesContainerView.placeholderView != placeholderView) {
    _badgesContainerView.placeholderView = placeholderView;
  }
  [self updateAccessibility];
}

- (void)setFullScreenCollapsedMode:(BOOL)isFullScreenCollapsed {
  if (!self.badgesContainerView.badgeView ||
      self.badgesContainerView.badgeView.hidden) {
    return;
  }

  if (isFullScreenCollapsed) {
    [NSLayoutConstraint
        activateConstraints:self.badgesViewFullScreenEnabledConstraints];
    [NSLayoutConstraint
        deactivateConstraints:self.badgesViewFullScreenDisabledConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:self.badgesViewFullScreenEnabledConstraints];
    [NSLayoutConstraint
        activateConstraints:self.badgesViewFullScreenDisabledConstraints];
  }
}

- (void)enableTrailingButton:(BOOL)enabled {
  self.trailingButton.enabled = enabled;
  [self updateAccessibility];
}

- (void)setCentered:(BOOL)centered {
  if (centered) {
    _xStickToLeadingSideConstraint.active = NO;
    // If the location label is currently being centered relative to content
    // around it, don't activate the following constraint (absolute centering).
    _xAbsoluteCenteredConstraint.active =
        !_xRelativeToContentCenteredConstraint.active;
  } else {
    _xAbsoluteCenteredConstraint.active = NO;
    _xStickToLeadingSideConstraint.active = YES;
  }

  // Call this in case the font was previously made smaller by the large
  // Contextual Panel entrypoint.
  _locationContainerView.transform = CGAffineTransformIdentity;
}

- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered {
  // Early return if the label is already justified to the leading edge, or if
  // the Contextual Panel entrypoint is not being shown.
  if (_xStickToLeadingSideConstraint.active ||
      (centered &&
       self.badgesContainerView.contextualPanelEntrypointView.hidden)) {
    _locationContainerView.transform = CGAffineTransformIdentity;
    return;
  }

  if (centered) {
    _xAbsoluteCenteredConstraint.active = NO;
    _xRelativeToContentCenteredConstraint.active = YES;

    // Make the location container smaller via transform to 1. allow animating
    // the "font" change and 2. make the entire location label container package
    // (label + image) become smaller momentarily.
    _locationContainerView.transform =
        CGAffineTransformMakeScale(kSmallerLocationLabelFontMultiplier,
                                   kSmallerLocationLabelFontMultiplier);
  } else {
    _xRelativeToContentCenteredConstraint.active = NO;
    _xAbsoluteCenteredConstraint.active = YES;
    _locationContainerView.transform = CGAffineTransformIdentity;
  }

  // This method is called as part of an animation, so layout here if needed.
  [self layoutIfNeeded];
}

- (id<ContextualPanelEntrypointVisibilityDelegate>)
    contextualEntrypointVisibilityDelegate {
  return self.badgesContainerView;
}

- (id<BadgeViewVisibilityDelegate>)badgeViewVisibilityDelegate {
  return self.badgesContainerView;
}

#pragma mark - UIResponder

// This is needed for UIMenu
- (BOOL)canBecomeFirstResponder {
  return true;
}

#pragma mark - UIView

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateFontOnTraitChange:previousTraitCollection];
}
#endif

#pragma mark - UIAccessibilityContainer

- (NSArray*)accessibilityElements {
  return self.accessibleElements;
}

- (NSInteger)accessibilityElementCount {
  return self.accessibleElements.count;
}

- (id)accessibilityElementAtIndex:(NSInteger)index {
  return self.accessibleElements[index];
}

- (NSInteger)indexOfAccessibilityElement:(id)element {
  return [self.accessibleElements indexOfObject:element];
}

#pragma mark - private

// Updates the location accessibility label and adds the correct views to
// accessible elements depending on their current displayed state.
- (void)updateAccessibility {
  [self.accessibleElements removeAllObjects];

  [_accessibleElements addObject:_locationButton];

  if (self.securityLevelAccessibilityString.length > 0) {
    self.locationButton.accessibilityValue =
        [NSString stringWithFormat:@"%@ %@", self.locationLabel.text,
                                   self.securityLevelAccessibilityString];
  } else {
    self.locationButton.accessibilityValue =
        [NSString stringWithFormat:@"%@", self.locationLabel.text];
  }

  [self.accessibleElements
      addObjectsFromArray:self.badgesContainerView.accessibilityElements];

  if (self.trailingButton && self.trailingButton.enabled) {
    [self.accessibleElements addObject:self.trailingButton];
  }
}

// Returns the normal font size for the location label.
- (UIFont*)locationLabelFont {
  return LocationBarSteadyViewFont(
      self.traitCollection.preferredContentSizeCategory);
}

- (void)updateTrailingButtonWithHighlightedStatus:(BOOL)highlighted {
  self.trailingButton.tintColor =
      highlighted ? [UIColor colorNamed:kSolidButtonTextColor]
                  : [UIColor colorNamed:kToolbarButtonColor];
  _trailingButtonSpotlightView.hidden = !highlighted;
}

// Updates the `locationLabel`'s font when the device's preferred content size
// category changes.
- (void)updateFontOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    self.locationLabel.font = [self locationLabelFont];
  }

  self.trailingButtonTrailingAnchorConstraint.constant =
      self.trailingButtonTrailingSpacing;
  [self layoutIfNeeded];
}

@end
