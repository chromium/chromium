// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view.h"

#import "base/check.h"
#import "base/check_op.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_view_visibility_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_visibility_delegate.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/location_bar/ui_bundled/badges_container_view.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Length of the trailing button side.
const CGFloat kButtonSize = 24;
// The offset to be applied to the centerig constraints when in incognito.
const CGFloat kIncognitoCenteringOffset = 3;
// Space between the incognito image and the location icon or label.
const CGFloat kIncognitoImageToLocationSpacing = 8;
// The size of the incognito image.
const CGFloat kIncognitoImageSize = 15;
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
// The duration of the custom leading view fade animation.
const CGFloat kCustomLeadingViewAnimationDuration = 0.3;
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
                     self.backgroundColor = [UIColor colorWithWhite:0
                                                              alpha:alpha];
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

  // The image view displaying the incognito icon.
  UIImageView* _incognitoImageView;

  // Whether the current text is a placeholder.
  BOOL _isShowingPlaceholder;

  // Spacing between the custom leading view and the URL label.
  CGFloat _customLeadingViewSpacing;

  // Target width for the custom leading view when visible.
  CGFloat _customLeadingViewTargetWidth;

  // Custom view added to the left of the location label.
  UIView* _customLeadingView;
  UIView* _customLeadingViewContainer;

  // Width constraint for custom leading view container (used for animation).
  NSLayoutConstraint* _customLeadingViewWidthConstraint;

  // Leading constraint for custom leading view (used for animation).
  NSLayoutConstraint* _customLeadingViewLeadingConstraint;

  // Array of active constraints for the content views inside
  // `locationContainerView`.
  NSArray<NSLayoutConstraint*>* _containerActiveConstraints;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self setUpViews];
    [self setUpLayout];
  }
  [self setUpAccessibility];
  return self;
}

- (void)updateCustomLeadingViewVisibility:(BOOL)visible
                                 animated:(BOOL)animated {
  CGFloat priorSpacing =
      [self shouldShowIncognitoBadge] ? kIncognitoImageToLocationSpacing : 0.0;
  CGFloat targetWidth = visible ? _customLeadingViewTargetWidth : 0.0;
  CGFloat targetSpacing =
      visible ? priorSpacing : (priorSpacing - _customLeadingViewSpacing);
  CGAffineTransform targetTransform =
      visible ? CGAffineTransformIdentity
              : CGAffineTransformMakeScale(0.01, 0.01);
  CGFloat targetAlpha = visible ? 1.0 : 0.0;

  if (!animated) {
    _customLeadingView.hidden = !visible;
    [self updateContainerConstraints];

    _customLeadingViewWidthConstraint.constant = targetWidth;
    _customLeadingViewLeadingConstraint.constant = targetSpacing;
    _customLeadingView.transform = targetTransform;
    _customLeadingView.alpha = targetAlpha;
    [self updateAccessibility];
    [self layoutIfNeeded];
    return;
  }

  if (visible) {
    _customLeadingView.hidden = NO;
    [self updateAccessibility];
    [self updateContainerConstraints];
  }

  NSLayoutConstraint* widthConstraint = _customLeadingViewWidthConstraint;
  NSLayoutConstraint* leadingConstraint = _customLeadingViewLeadingConstraint;
  UIView* customLeadingView = _customLeadingView;
  __weak LocationBarSteadyView* weakSelf = self;

  [UIView animateWithDuration:kCustomLeadingViewAnimationDuration
      animations:^{
        widthConstraint.constant = targetWidth;
        leadingConstraint.constant = targetSpacing;
        customLeadingView.transform = targetTransform;
        customLeadingView.alpha = targetAlpha;
        [weakSelf layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        if (!visible && finished) {
          customLeadingView.hidden = YES;
          [weakSelf updateAccessibility];
          [weakSelf updateContainerConstraints];
        }
      }];
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

  // Setup label.
  _locationLabel.lineBreakMode = NSLineBreakByTruncatingHead;
  _locationLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [_locationLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  if (IsChromeNextIaEnabled()) {
    _locationLabel.font =
        PreferredFontForTextStyle(UIFontTextStyleCallout, UIFontWeightMedium);
  } else {
    _locationLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  }
  _locationLabel.adjustsFontForContentSizeCategory = YES;
  _locationLabel.maximumContentSizeCategory =
      IsChromeNextIaEnabled() ? LocationBarSteadyViewMaxSizeCategory()
                              : LegacyLocationBarSteadyViewMaxSizeCategory();

  // Container for location label and icon.
  _locationContainerView = [[UIView alloc] init];
  _locationContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _locationContainerView.userInteractionEnabled = NO;
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
  [self updateContainerConstraints];

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

  // Setup the layout guide centered between the contents of the location
  // bar.
  _centeredBetweenLocationBarContentsLayoutGuide = [[UILayoutGuide alloc] init];
  [_locationButton
      addLayoutGuide:_centeredBetweenLocationBarContentsLayoutGuide];
  [NSLayoutConstraint activateConstraints:@[
    [_centeredBetweenLocationBarContentsLayoutGuide.leadingAnchor
        constraintEqualToAnchor:_badgesContainerView.trailingAnchor],
    [_centeredBetweenLocationBarContentsLayoutGuide.trailingAnchor
        constraintEqualToAnchor:_trailingButton.leadingAnchor],
  ]];

  _xRelativeToContentCenteredConstraint = [_locationContainerView.centerXAnchor
      constraintEqualToAnchor:_centeredBetweenLocationBarContentsLayoutGuide
                                  .centerXAnchor];
  _xRelativeToContentCenteredConstraint.priority =
      UILayoutPriorityDefaultHigh - 1;

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

- (void)setUpAccessibility {
  // Setup accessibility.
  _trailingButton.isAccessibilityElement = YES;
  _locationButton.isAccessibilityElement = YES;
  _locationButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_LOCATION);

  // These two elements must remain accessible for egtests, but will not be
  // included in accessibility navigation as they are not added to the
  // accessibleElements array.
  _locationIconImageView.isAccessibilityElement = YES;
  _locationLabel.isAccessibilityElement = YES;

  _accessibleElements = [[NSMutableArray alloc] init];
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
  _incognitoImageView.tintColor = self.colorScheme.fontColor;
}

- (void)setLocationImage:(UIImage*)locationImage {
  BOOL hadImage = self.locationIconImageView.image != nil;
  BOOL hasImage = locationImage != nil;
  self.locationIconImageView.image = locationImage;
  if (hadImage == hasImage) {
    return;
  }

  [self updateContainerConstraints];
}

- (void)addCustomLeadingView:(UIView*)view
                 targetWidth:(CGFloat)targetWidth
                     spacing:(CGFloat)spacing {
  // Clean up if the icon is already set.
  if (_customLeadingViewContainer &&
      [_customLeadingViewContainer isDescendantOfView:self]) {
    [_customLeadingViewContainer removeFromSuperview];
  }
  _customLeadingView = view;
  _customLeadingViewSpacing = spacing;
  _customLeadingViewTargetWidth = targetWidth;

  // Ensure accessibility is correctly configured on the custom leading view.
  _customLeadingView.isAccessibilityElement = YES;
  if (!_customLeadingView.accessibilityLabel) {
    _customLeadingView.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_GEMINI_LIVE_ACCESSIBILITY_LABEL);
  }

  _customLeadingViewContainer = [[UIView alloc] init];
  _customLeadingViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _customLeadingViewContainer.clipsToBounds = NO;

  [_customLeadingViewContainer addSubview:_customLeadingView];
  _customLeadingView.translatesAutoresizingMaskIntoConstraints = NO;

  // Constrain the child view tightly inside its container.
  [NSLayoutConstraint activateConstraints:@[
    [_customLeadingView.leadingAnchor
        constraintEqualToAnchor:_customLeadingViewContainer.leadingAnchor],
    [_customLeadingView.centerYAnchor
        constraintEqualToAnchor:_customLeadingViewContainer.centerYAnchor],
    [_customLeadingView.widthAnchor
        constraintEqualToConstant:_customLeadingViewTargetWidth],
    [_customLeadingViewContainer.heightAnchor
        constraintEqualToAnchor:_customLeadingView.heightAnchor],
  ]];

  _customLeadingViewWidthConstraint =
      [_customLeadingViewContainer.widthAnchor constraintEqualToConstant:0.0];
  _customLeadingViewWidthConstraint.active = YES;

  _customLeadingView.transform = CGAffineTransformMakeScale(0.01, 0.01);
  _customLeadingView.alpha = 0.0;
  _customLeadingView.hidden = YES;

  [self.locationContainerView addSubview:_customLeadingViewContainer];
  [self updateContainerConstraints];
  [self updateAccessibility];
}

- (void)setLocationLabelText:(NSString*)string {
  [self setLocationLabelText:string clipTail:NO];
}

- (void)setLocationLabelText:(NSString*)string clipTail:(BOOL)clipTail {
  _isShowingPlaceholder = NO;
  // Use attributed text to force LTR direction for URLs, preventing RTL
  // characters from messing up the visual order (e.g. IDN with RTL scripts).
  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/url_display_guidelines/url_display_guidelines.md#rtl
  [style setBaseWritingDirection:NSWritingDirectionLeftToRight];
  [style setLineBreakMode:clipTail ? NSLineBreakByTruncatingTail
                                   : NSLineBreakByTruncatingHead];

  NSDictionary* attributes = @{NSParagraphStyleAttributeName : style};

  self.locationLabel.attributedText =
      [[NSAttributedString alloc] initWithString:string attributes:attributes];
  self.locationLabel.textColor = self.colorScheme.fontColor;
  [self updateAccessibility];
  [self updateContainerConstraints];
}

- (void)setLocationLabelPlaceholderText:(NSString*)string {
  _isShowingPlaceholder = YES;
  self.locationLabel.textColor = self.colorScheme.placeholderColor;
  self.locationLabel.text = string;
  [self updateContainerConstraints];
}

- (void)setSecurityLevelAccessibilityString:(NSString*)string {
  if ([_securityLevelAccessibilityString isEqualToString:string]) {
    return;
  }
  _securityLevelAccessibilityString = [string copy];
  [self updateAccessibility];
}

- (void)setIncognitoBadgeView:(UIView*)incognitoBadgeView {
  BOOL hadBadgeView = _badgesContainerView.incognitoBadgeView != nil;
  if (!hadBadgeView && incognitoBadgeView) {
    _badgesContainerView.incognitoBadgeView = incognitoBadgeView;
  }
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

- (void)setReaderModeChipView:(UIView*)readerModeChipView {
  if (!_badgesContainerView.readerModeChipView && readerModeChipView) {
    _badgesContainerView.readerModeChipView = readerModeChipView;
  }
  [self updateAccessibility];
}

- (void)setPlaceholderView:(UIView*)placeholderView
                      type:(LocationBarPlaceholderType)placeholderType {
  if (_badgesContainerView.placeholderView != placeholderView) {
    _badgesContainerView.placeholderType = placeholderType;
    _badgesContainerView.placeholderView = placeholderView;
  }
  [self updateAccessibility];
}

- (void)setPageActionMenuHandler:
    (id<PageActionMenuCommands>)pageActionMenuHandler {
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    _pageActionMenuHandler = pageActionMenuHandler;
    _badgesContainerView.pageActionMenuHandler = pageActionMenuHandler;
  }
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

- (id<ReaderModeChipVisibilityDelegate>)readerModeChipVisibilityDelegate {
  return self.badgesContainerView;
}

- (id<BadgeViewVisibilityDelegate>)badgeViewVisibilityDelegate {
  return self.badgesContainerView;
}

- (id<IncognitoBadgeViewVisibilityDelegate>)
    incognitoBadgeViewVisibilityDelegate {
  return self.badgesContainerView;
}

#pragma mark - UIResponder

// This is needed for UIMenu
- (BOOL)canBecomeFirstResponder {
  return true;
}

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

  if (_customLeadingView && !_customLeadingView.hidden) {
    [self.accessibleElements addObject:_customLeadingView];
  }

  [_accessibleElements addObject:_locationButton];

  if ([self shouldShowIncognitoBadge]) {
    [self.accessibleElements addObject:_incognitoImageView];
  }

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

// Propagates the incognito state to the badges container view.
- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  if (_incognito && !_incognitoImageView) {
    _incognitoImageView = [[UIImageView alloc] init];
    _incognitoImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [_incognitoImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _incognitoImageView.isAccessibilityElement = YES;
    _incognitoImageView.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BADGE_INCOGNITO_HINT);
    UIImageConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:kIncognitoImageSize
                            weight:UIImageSymbolWeightBold
                             scale:UIImageSymbolScaleMedium];
    _incognitoImageView.image =
        CustomSymbolWithConfiguration(kIncognitoSymbol, configuration);
    _incognitoImageView.tintColor = self.colorScheme.fontColor;
  }

  self.badgesContainerView.incognito = incognito;
  [self updateContainerConstraints];
}

// Updates the current constraints.
- (void)updateContainerConstraints {
  [NSLayoutConstraint deactivateConstraints:_containerActiveConstraints];

  BOOL hasIncognito = [self shouldShowIncognitoBadge];
  BOOL hasLocationImage = self.locationIconImageView.image != nil;

  if (hasIncognito) {
    [self.locationButton addSubview:_incognitoImageView];
  } else {
    [_incognitoImageView removeFromSuperview];
  }

  if (hasLocationImage) {
    [self.locationContainerView addSubview:self.locationIconImageView];
  } else {
    [self.locationIconImageView removeFromSuperview];
  }

  NSMutableArray* constraints = [[NSMutableArray alloc] init];

  if (hasIncognito) {
    [constraints addObjectsFromArray:@[
      [_incognitoImageView.centerYAnchor
          constraintEqualToAnchor:_locationContainerView.centerYAnchor],
      [_locationContainerView.leadingAnchor
          constraintEqualToAnchor:_incognitoImageView.leadingAnchor]
    ]];
    _xAbsoluteCenteredConstraint.constant = -kIncognitoCenteringOffset;
  } else {
    _xAbsoluteCenteredConstraint.constant = 0;
  }

  // Pin label to trailing edge.
  [constraints addObject:[_locationLabel.trailingAnchor
                             constraintEqualToAnchor:_locationContainerView
                                                         .trailingAnchor]];

  NSLayoutXAxisAnchor* leadingTargetAnchor =
      _locationContainerView.leadingAnchor;
  CGFloat currentSpacing = 0.0;

  if (hasIncognito) {
    leadingTargetAnchor = _incognitoImageView.trailingAnchor;
    currentSpacing = kIncognitoImageToLocationSpacing;
  }

  if (_customLeadingViewContainer && !_customLeadingView.hidden) {
    _customLeadingViewLeadingConstraint =
        [_customLeadingViewContainer.leadingAnchor
            constraintEqualToAnchor:leadingTargetAnchor
                           constant:currentSpacing];
    [constraints addObjectsFromArray:@[
      _customLeadingViewLeadingConstraint,
      [_customLeadingViewContainer.centerYAnchor
          constraintEqualToAnchor:_locationContainerView.centerYAnchor],
    ]];
    leadingTargetAnchor = _customLeadingViewContainer.trailingAnchor;
    currentSpacing = _customLeadingViewSpacing;
  }

  if (hasLocationImage) {
    [constraints addObjectsFromArray:@[
      [self.locationIconImageView.leadingAnchor
          constraintEqualToAnchor:leadingTargetAnchor
                         constant:currentSpacing],
      [self.locationIconImageView.centerYAnchor
          constraintEqualToAnchor:_locationContainerView.centerYAnchor],
    ]];
    leadingTargetAnchor = self.locationIconImageView.trailingAnchor;
    currentSpacing = -kLocationImageToLabelSpacing;
  }

  [constraints addObject:[_locationLabel.leadingAnchor
                             constraintEqualToAnchor:leadingTargetAnchor
                                            constant:currentSpacing]];

  _containerActiveConstraints = constraints;
  [NSLayoutConstraint activateConstraints:_containerActiveConstraints];
}

// Whether the incognito badge should be visible or not.
- (BOOL)shouldShowIncognitoBadge {
  return self.isIncognito && IsChromeNextIaEnabled() && !_isShowingPlaceholder;
}

@end
