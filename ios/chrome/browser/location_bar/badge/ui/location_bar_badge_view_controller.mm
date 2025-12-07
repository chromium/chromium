// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_view_controller.h"

#import "base/i18n/rtl.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_view_controller.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_util.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_mutator.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_mutator.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_metrics.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_visibility_delegate.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

// Sets `view.hidden` to `hidden` if necessary. This helper is useful to address
// a bug where the number of times `.hidden` is set in a view accumulates if it
// is presented inside of a stack view. As a result, setting `.hidden = YES`
// twice does not have the same effect as only settings it once.
void HideViewIfNecessary(UIView* view, BOOL hidden) {
  if (view.hidden != hidden) {
    view.hidden = hidden;
  }
}

// Height of `unreadIndicatorView`.
const CGFloat kUnreadIndicatorViewHeight = 6.0;

// Leading space for the separator that displays after the badge.
const CGFloat kLeadingSeparatorSpace = 5.0;

}  // anonymous namespace

@implementation LocationBarBadgeViewController {
  /// Whether the contextual panel badge should be visible. The placeholder
  /// view trumps the entrypoint when kLensOverlayPriceInsightsCounterfactual is
  /// enabled.
  BOOL _contextualPanelEntrypointShouldBeVisible;
  // Whether the location bar badge should be visible.
  BOOL _locationBarBadgeShouldBeVisible;
  /// Whether the incognito badge view should be visible.
  BOOL _incognitoBadgeViewShouldBeVisible;
  // A horizontal stack view for different badge setups such as incognito badge
  // with other badges.
  UIStackView* _badgeStackView;
  // The injected view displaying the incognito badge. Nil for non-incognito.
  UIView* _incognitoBadgeView;
  // The UIButton contains a UIView, which itself contains the
  // badge's image and label. The container (UIButton) is needed for
  // button-like behavior and to create the shadow around the entire
  // entrypoint package. The content UIView is needed to populate the inner
  // contents of the button and to clip the label to the badge's bounds for
  // proper animations and sizing.
  UIButton* _buttonContainer;
  UIView* _badgeContentView;
  UIImageView* _badgeIcon;
  UILabel* _label;
  // The small vertical pill-shaped line separating the Location Bar Badge
  // entrypoint and Infobar badges, if present.
  UIView* _separator;
  // Constraints for the two states of the trailing edge of the badge
  // container. They are activated/deactivated as needed when the label is
  // shown/hidden.
  NSLayoutConstraint* _expandedContainerTrailingConstraint;
  NSLayoutConstraint* _collapsedContainerTrailingConstraint;
  // Constraint for default leading view. By default, the leading view is
  // `leadingSpace`. In incognito, the leading view is
  // `incognitoBadgeView`.
  NSLayoutConstraint* _defaultLeadingViewConstraint;
  // Whether the badge is tapped. Used to update the badge's colors.
  BOOL _badgeTapped;
  // Whether the entrypoint should currently collapse for fullscreen.
  BOOL _shouldCollapseForFullscreen;
  // Whether there currently are any Infobar badges being shown.
  BOOL _infobarBadgesCurrentlyShown;
  // LayoutGuideCenter to register the entrypoint container's view for global
  // access, only when it is large (i.e. dismissable).
  LayoutGuideCenter* _layoutGuideCenter;
  // Swipe gesture recognizer for the entrypoint (allows the user to "dismiss"
  // the large chip entrypoint).
  UISwipeGestureRecognizer* _swipeRecognizer;
  // Configuration for updating the badge.
  LocationBarBadgeConfiguration* _badgeConfig;
  // View that displays a blue dot on the top-right corner of the displayed
  // badge if there are unread badges to be shown in the overflow menu.
  UIView* _unreadIndicatorView;
}

#pragma mark - Public

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set the view as hidden when created as it should only appear when the
  // entrypoint should be shown.
  self.view.hidden = YES;
  self.view.isAccessibilityElement = NO;
  _locationBarBadgeShouldBeVisible = NO;

  _badgeStackView = [[UIStackView alloc] init];
  _badgeStackView.isAccessibilityElement = NO;
  _badgeStackView.axis = UILayoutConstraintAxisHorizontal;
  _badgeStackView.alignment = UIStackViewAlignmentCenter;
  _badgeStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_badgeStackView];

  // Setup for capsule badges and chips.
  _buttonContainer = [self configuredButtonContainer];
  _badgeContentView = [self configuredBadgeContentView];
  _badgeIcon = [self configuredBadgeIcon];
  _label = [self configuredLabel];
  _separator = [self configuredSeparator];

  [_buttonContainer addSubview:_badgeContentView];
  [_badgeContentView addSubview:_badgeIcon];
  [_badgeContentView addSubview:_label];
  [_badgeStackView addArrangedSubview:_buttonContainer];
  [_badgeStackView setCustomSpacing:kLeadingSeparatorSpace
                          afterView:_buttonContainer];
  [_badgeStackView addArrangedSubview:_separator];

  [self updateAccessibilityStatus];

  [self activateInitialConstraints];

  [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                     withAction:@selector(updateLabelFont)];

  // TODO(crbug.com/361110974): Have bubbles gracefully handle orientation
  // changes without needing to dismiss here.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(dismissIPHWithoutAnimation)
                 name:UIDeviceOrientationDidChangeNotification
               object:nil];

  [NSLayoutConstraint activateConstraints:@[
    [self.view.heightAnchor constraintEqualToAnchor:super.view.heightAnchor],
  ]];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  _badgeContentView.layer.cornerRadius =
      _badgeContentView.bounds.size.height / 2.0;

  _separator.layer.cornerRadius = _separator.bounds.size.width / 2.0;
}

- (CGPoint)helpAnchorUsingBottomOmnibox:(BOOL)isBottomOmnibox {
  CGPoint anchorPointInSuperview =
      CGPointMake(CGRectGetMidX(_buttonContainer.bounds),
                  isBottomOmnibox ? CGRectGetMinY(_buttonContainer.bounds)
                                  : CGRectGetMaxY(_buttonContainer.bounds));
  CGPoint anchorPointInWindow =
      [self.view.window convertPoint:anchorPointInSuperview
                            fromView:_buttonContainer];

  // The default bubble alignment is the minimum distance from the edge of the
  // window that the bubble can appear at, so use MAX (or MIN in RTL) between
  // that and the MidX of the entrypoint container.
  anchorPointInWindow.x =
      base::i18n::IsRTL() ? MIN(self.view.window.bounds.size.width -
                                    bubble_util::BubbleDefaultAlignmentOffset(),
                                anchorPointInWindow.x)
                          : MAX(bubble_util::BubbleDefaultAlignmentOffset(),
                                anchorPointInWindow.x);

  return anchorPointInWindow;
}

#pragma mark - Setters

- (void)setIncognitoBadgeViewController:
    (IncognitoBadgeViewController*)incognitoViewController {
  incognitoViewController.visibilityDelegate = self;
  _incognitoBadgeView = incognitoViewController.view;
  _incognitoBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
  _incognitoBadgeView.isAccessibilityElement = NO;
  [_badgeStackView insertArrangedSubview:_incognitoBadgeView atIndex:0];
  HideViewIfNecessary(_incognitoBadgeView, YES);
  [self addChildViewController:incognitoViewController];
  [incognitoViewController didMoveToParentViewController:self];
  _incognitoBadgeViewController = incognitoViewController;

  _defaultLeadingViewConstraint.active = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_incognitoBadgeView.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor],
  ]];
}

#pragma mark - LocationBarBadgeConsumer

// TODO(crbug.com/448422022): Trigger visibility refresh when a new badge comes
// in and store the badge for multi-badge setup.
- (void)setBadgeConfig:(LocationBarBadgeConfiguration*)config {
  if (!config) {
    return;
  }

  _buttonContainer.accessibilityLabel = config.accessibilityLabel;
  if (config.accessibilityHint) {
    _buttonContainer.accessibilityHint = config.accessibilityHint;
  }

  if (config.badgeText) {
    _label.text = config.badgeText;
  }

  _badgeIcon.image = config.badgeImage;
  _badgeConfig = config;
}

#pragma mark - ContextualPanelEntrypointVisibilityDelegate

// TODO(crbug.com/429140788): Remove after migration and BadgesContainerView
// is obsolete. Should not be used to update Location Bar Badge visibility.
// Should instead use `setLocationBarBadgeHidden`.
- (void)setContextualPanelEntrypointHidden:(BOOL)hidden {
  _contextualPanelEntrypointShouldBeVisible = !hidden;
}

- (void)setContextualPanelItemType:
    (std::optional<ContextualPanelItemType>)itemType {
  [self.visibilityDelegate setContextualPanelItemType:itemType];
}

- (void)setContextualPanelCurrentlyAnimating:(BOOL)animating {
  [self.visibilityDelegate setContextualPanelCurrentlyAnimating:animating];
}

#pragma mark - IncognitoBadgeViewVisibilityDelegate

- (void)setIncognitoBadgeViewHidden:(BOOL)hidden {
  if (_incognitoBadgeViewShouldBeVisible == !hidden) {
    return;
  }

  _incognitoBadgeViewShouldBeVisible = !hidden;
  [self setLocationBarBadgeHidden:hidden];
}

#pragma mark - Private

// Updates the hidden state of the views.
- (void)updateViewsVisibility {
  if (_incognitoBadgeView && !_incognitoBadgeViewShouldBeVisible) {
    HideViewIfNecessary(_badgeStackView, YES);
  } else {
    HideViewIfNecessary(_badgeStackView, !_locationBarBadgeShouldBeVisible);
  }

  // Whether the default/placeholder badge should show. Only shown if no other
  // badge or chip is shown.
  BOOL placeholderHidden = self.view && !_badgeConfig;

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
    } else if ([_badgeConfig fromBadgeFactory]) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kMessage);
    } else if (_badgeConfig.badgeType == LocationBarBadgeType::kReaderMode) {
      RecordLensEntrypointHidden(IOSLocationBarLeadingIconType::kReaderMode);
    }
  }
}

// Returns the button configuration with the given background color.
- (UIButtonConfiguration*)buttonConfigurationWithBackgroundColor:
    (UIColor*)backgroundColor {
  UIButtonConfiguration* configuration;

  if ([self useMultiBadge]) {
    configuration = [UIButtonConfiguration plainButtonConfiguration];
  } else {
    configuration = [UIButtonConfiguration filledButtonConfiguration];
    configuration.baseBackgroundColor = backgroundColor;
  }

  configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  return configuration;
}

// Creates and configures the button container.
- (UIButton*)configuredButtonContainer {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  UIColor* defaultBackgroundColor = [self useMultiBadge]
                                        ? [UIColor clearColor]
                                        : [UIColor colorNamed:kBackgroundColor];
  button.configuration =
      [self buttonConfigurationWithBackgroundColor:defaultBackgroundColor];
  button.clipsToBounds = NO;
  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();

  // Configure shadow.
  button.layer.shadowColor = [[UIColor blackColor] CGColor];
  button.layer.shadowOpacity = kBadgeContainerShadowOpacity;
  button.layer.shadowOffset = kBadgeContainerShadowOffset;
  button.layer.shadowRadius = kBadgeContainerShadowRadius;
  button.layer.masksToBounds = NO;

  [button addTarget:self
                action:@selector(userTappedBadge)
      forControlEvents:UIControlEventTouchUpInside];

  return button;
}

// Creates and configures the badge's content view which mirrors the
// container view but adds clipping to bounds.
- (UIView*)configuredBadgeContentView {
  UIView* view = [[UIView alloc] init];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  view.userInteractionEnabled = NO;
  view.clipsToBounds = YES;

  return view;
}

// Creates and configures the badge icon.
- (UIImageView*)configuredBadgeIcon {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.isAccessibilityElement = NO;
  imageView.contentMode = UIViewContentModeCenter;
  imageView.tintColor = [UIColor colorNamed:kBlue600Color];
  imageView.accessibilityIdentifier = kLocationBarBadgeImageViewIdentifier;
  [imageView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];

  CGFloat symbolPointSize = kBadgeSymbolPointSize;

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:symbolPointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  imageView.preferredSymbolConfiguration = symbolConfig;

  return imageView;
}

// Creates and configures the label for louder moments. Starts off as hidden.
- (UILabel*)configuredLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [self badgeLabelFont];
  label.numberOfLines = 1;
  label.accessibilityIdentifier = kLocationBarBadgeLabelIdentifier;
  label.isAccessibilityElement = NO;
  [label
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];

  return label;
}

// Creates and configures the button's pill-shaped separator (vertical
// line).
- (UIView*)configuredSeparator {
  UIView* view = [[UIView alloc] init];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  view.isAccessibilityElement = NO;
  view.backgroundColor = [UIColor colorNamed:kGrey400Color];

  return view;
}

- (void)activateInitialConstraints {
  // Leading space before the start of the button container view.
  UILayoutGuide* leadingSpace = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:leadingSpace];

  UILayoutGuide* labelLeadingSpace = [[UILayoutGuide alloc] init];
  UILayoutGuide* labelTrailingSpace = [[UILayoutGuide alloc] init];

  [_badgeContentView addLayoutGuide:labelLeadingSpace];
  [_badgeContentView addLayoutGuide:labelTrailingSpace];

  _collapsedContainerTrailingConstraint = [_buttonContainer.trailingAnchor
      constraintEqualToAnchor:_badgeIcon.trailingAnchor];
  _expandedContainerTrailingConstraint = [_buttonContainer.trailingAnchor
      constraintEqualToAnchor:labelTrailingSpace.trailingAnchor];
  _defaultLeadingViewConstraint = [leadingSpace.leadingAnchor
      constraintEqualToAnchor:_badgeStackView.leadingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [self.view.widthAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.heightAnchor],
    _collapsedContainerTrailingConstraint,
    // The badge doesn't fully fill the height of the location bar, so to
    // make it exactly follow the curvature of the location bar's corner radius,
    // it must be placed with the same amount of margin space horizontally that
    // exists vertically between the entrypoint and the location bar itself.
    [leadingSpace.widthAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:((1 - kBadgeHeightMultiplier) / 2)],
    [leadingSpace.trailingAnchor
        constraintEqualToAnchor:_buttonContainer.leadingAnchor],
    _defaultLeadingViewConstraint,
    [_badgeStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_badgeStackView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_badgeStackView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [_separator.widthAnchor constraintEqualToConstant:kSeparatorWidthConstant],
    [_separator.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kSeparatorHeightMultiplier],
    [_buttonContainer.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kBadgeHeightMultiplier],
    [self.view.trailingAnchor
        constraintGreaterThanOrEqualToAnchor:_badgeStackView.trailingAnchor],
    [_badgeIcon.heightAnchor
        constraintEqualToAnchor:_buttonContainer.heightAnchor],
    [_badgeIcon.widthAnchor constraintEqualToAnchor:_badgeIcon.heightAnchor],
    [_badgeIcon.leadingAnchor
        constraintEqualToAnchor:_buttonContainer.leadingAnchor],
    [_badgeIcon.centerYAnchor
        constraintEqualToAnchor:_buttonContainer.centerYAnchor],
    [labelLeadingSpace.leadingAnchor
        constraintEqualToAnchor:_badgeIcon.trailingAnchor],
    [labelLeadingSpace.widthAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kLabelLeadingSpaceMultiplier],
    [labelTrailingSpace.widthAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kLabelTrailingSpaceMultiplier],
    [labelTrailingSpace.leadingAnchor
        constraintEqualToAnchor:_label.trailingAnchor],
    [_label.heightAnchor constraintEqualToAnchor:_buttonContainer.heightAnchor],
    [_label.centerYAnchor
        constraintEqualToAnchor:_buttonContainer.centerYAnchor],
    [_label.leadingAnchor
        constraintEqualToAnchor:labelLeadingSpace.trailingAnchor],
  ]];

  AddSameConstraints(_badgeContentView, _buttonContainer);
}

- (void)activateExpandedContainerConstraint {
  _collapsedContainerTrailingConstraint.active = NO;
  _expandedContainerTrailingConstraint.active = YES;
}

- (void)activateCollapsedContainerConstraint {
  _expandedContainerTrailingConstraint.active = NO;
  _collapsedContainerTrailingConstraint.active = YES;
}

- (void)updateLabelFont {
  _label.font = [self badgeLabelFont];
}

- (void)dismissIPHWithoutAnimation {
  if ([_badgeConfig isContextualPanelEntrypointBadge]) {
    [self.contextualPanelEntryPointMutator dismissIPHAnimated:NO];
  } else {
    [self.mutator dismissIPHAnimated:NO];
  }
}

// Returns the preferred font and size given the current ContentSizeCategory.
- (UIFont*)badgeLabelFont {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleFootnote,
      self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityLarge);
}

// Refreshes the VoiceOver bounding box and notifies the mutator that the
// animation to collapse the badge container is complete.
- (void)didCollapseBadgeContainer {
  [self refreshVoiceOverBoundingBoxIfFocused];
  if ([_badgeConfig isContextualPanelEntrypointBadge]) {
    [self.contextualPanelEntryPointMutator
            didCompleteTransitionToSmallEntrypoint];
  } else {
    [self.mutator handleBadgeContainerCollapse:_badgeConfig.badgeType];
  }

  if (_badgeConfig.shouldHideBadgeAfterChipCollapse) {
    [self hideBadge];
  }
}

// Sets the proper visual features depending on current infobar badges status
// and whether the Location Bar Badge is open.
- (void)refreshEntrypointVisualElements {
  BOOL shouldAccountForVisibleInfobarBadges =
      _infobarBadgesCurrentlyShown && !IsReaderModeAvailable();
  BOOL shouldShowMutedColors =
      shouldAccountForVisibleInfobarBadges || _badgeTapped;
  BOOL isInUnifiedContainer = [self useMultiBadge] && [self isBadgeVisible];

  // Badge icon tint color.
  if (isInUnifiedContainer) {
    _badgeIcon.tintColor = [UIColor whiteColor];
  } else {
    _badgeIcon.tintColor = shouldShowMutedColors
                               ? [UIColor colorNamed:kGrey600Color]
                               : [self defaultBadgeTintColor];
  }

  // Button container shadow.
  if (isInUnifiedContainer || shouldShowMutedColors) {
    _buttonContainer.layer.shadowOpacity = 0;
  } else {
    _buttonContainer.layer.shadowOpacity = kBadgeContainerShadowOpacity;
  }

  // Button container background color.
  UIColor* buttonContainerBackgroundColor;
  if (isInUnifiedContainer) {
    buttonContainerBackgroundColor = [UIColor clearColor];
  } else {
    UIColor* untappedBackgroundColor =
        shouldAccountForVisibleInfobarBadges
            ? nil
            : [UIColor colorNamed:kBackgroundColor];
    buttonContainerBackgroundColor = _badgeTapped
                                         ? [UIColor colorNamed:kGrey100Color]
                                         : untappedBackgroundColor;
  }
  _buttonContainer.configuration = [self
      buttonConfigurationWithBackgroundColor:buttonContainerBackgroundColor];

  // Separator visibility.
  _separator.hidden = !_infobarBadgesCurrentlyShown;
}

// Applies the correct color to the badge (highlighted blue when the
// in-product help is present), otherwise back to the normal colorset.
- (void)updateBadgeHighlight:(BOOL)highlighted {
  _badgeIcon.tintColor = highlighted ? [UIColor colorNamed:kBackgroundColor]
                                     : [self defaultBadgeTintColor];

  // Update entrypoint container background.
  UIColor* buttonContainerBackgroundColor =
      highlighted ? [UIColor colorNamed:kBlue600Color]
                  : [UIColor colorNamed:kBackgroundColor];
  _buttonContainer.configuration = [self
      buttonConfigurationWithBackgroundColor:buttonContainerBackgroundColor];
}

// Returns the default badge tint color. Ignores applying a tint color in favor
// of using an image gradient layer.
- (UIColor*)defaultBadgeTintColor {
  BOOL useImageGradient =
      _badgeConfig.badgeType == LocationBarBadgeType::kGeminiContextualCueChip;
  return useImageGradient ? nil : [UIColor colorNamed:kBlue600Color];
}

// User swiped the expanded badge towards the leading edge to dismiss it.
- (void)expandedBadgeSwiped {
  [self collapseBadgeContainer];
  // TODO(crbug.com/450006763): Create and use a metric for Location Bar Badge.
  base::RecordAction(base::UserMetricsAction(
      "IOSContextualPanelEntrypointLargeChipDismissedWithSwipe"));
}

// Refreshes the VoiceOver bounding box if VoiceOver is currently running and
// the entrypoint is focused.
- (void)refreshVoiceOverBoundingBoxIfFocused {
  if (!UIAccessibilityIsVoiceOverRunning() ||
      ![_buttonContainer accessibilityElementIsFocused]) {
    return;
  }

  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  _buttonContainer);
}

// Notify that a user tapped the badge.
- (void)userTappedBadge {
  _badgeTapped = YES;
  [self refreshEntrypointVisualElements];
  [self collapseBadgeContainer];
  if ([_badgeConfig isContextualPanelEntrypointBadge]) {
    [self.contextualPanelEntryPointMutator entrypointTapped];
  } else {
    [self.mutator badgeTapped:_badgeConfig];
  }
}

// Sets visibility for location bar badge in the omnibox.
- (void)setLocationBarBadgeHidden:(BOOL)hidden {
  _locationBarBadgeShouldBeVisible = !hidden;
  self.view.hidden = hidden;
  [self updateViewsVisibility];
  // TODO(crbug.com/429140788): Remove after migration and BadgesContainerView
  // is obsolete.
  [self.visibilityDelegate setContextualPanelEntrypointHidden:hidden];
}

// Sets center positioning for location bar label.
- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered {
  if ([_badgeConfig isContextualPanelEntrypointBadge]) {
    [self.contextualPanelEntryPointMutator
        setLocationBarLabelCenteredBetweenContent:centered];
  } else {
    [self.mutator setLocationBarLabelCenteredBetweenContent:centered];
  }
}

- (BOOL)useMultiBadge {
  return IsProactiveSuggestionsFrameworkEnabled() &&
         _badgeConfig.badgeType !=
             LocationBarBadgeType::kGeminiContextualCueChip;
}

#pragma mark - ContextualPanelEntrypointConsumer

- (void)setEntrypointConfig:(ContextualPanelItemConfiguration*)config {
  if (IsAskGeminiChipEnabled()) {
    LocationBarBadgeType badgeType;
    switch (config->item_type) {
      case ContextualPanelItemType::SamplePanelItem:
        badgeType = LocationBarBadgeType::kContextualPanelEntryPointSample;
        break;
      case ContextualPanelItemType::PriceInsightsItem:
        badgeType = LocationBarBadgeType::kPriceInsights;
        break;
      case ContextualPanelItemType::ReaderModeItem:
        badgeType = LocationBarBadgeType::kReaderMode;
        break;
    }

    RecordLocationBarBadgeUpdate(badgeType);
    // TODO(crbug.com/448422022): Store Contextual Panel Entrypoint badges
    // instead of preventing them.
    if (_locationBarBadgeShouldBeVisible) {
      return;
    }

    NSString* accessibilityLabel =
        base::SysUTF8ToNSString(config->accessibility_label);

    UIImage* image;
    CGFloat symbolPointSize = kBadgeSymbolPointSize;
    switch (config->image_type) {
      case ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol:
        image = DefaultSymbolWithPointSize(
            base::SysUTF8ToNSString(config->entrypoint_image_name),
            symbolPointSize);
        break;
      case ContextualPanelItemConfiguration::EntrypointImageType::Image:
        image = CustomSymbolWithPointSize(
            base::SysUTF8ToNSString(config->entrypoint_image_name),
            symbolPointSize);
        break;
    }

    LocationBarBadgeConfiguration* badgeConfig =
        [[LocationBarBadgeConfiguration alloc]
             initWithBadgeType:badgeType
            accessibilityLabel:accessibilityLabel
                    badgeImage:image];
    badgeConfig.badgeText = base::SysUTF8ToNSString(config->entrypoint_message);

    if (config->accessibility_hint.size() > 0) {
      badgeConfig.accessibilityHint =
          base::SysUTF8ToNSString(config->accessibility_hint);
    }

    [self setBadgeConfig:badgeConfig];
  } else {
    if (!config) {
      return;
    }

    _buttonContainer.accessibilityLabel =
        base::SysUTF8ToNSString(config->accessibility_label);
    if (config->accessibility_hint.size() > 0) {
      _buttonContainer.accessibilityHint =
          base::SysUTF8ToNSString(config->accessibility_hint);
    }

    _label.text = base::SysUTF8ToNSString(config->entrypoint_message);

    UIImage* image;
    CGFloat symbolPointSize = kBadgeSymbolPointSize;
    switch (config->image_type) {
      case ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol:
        image = DefaultSymbolWithPointSize(
            base::SysUTF8ToNSString(config->entrypoint_image_name),
            symbolPointSize);
        break;
      case ContextualPanelItemConfiguration::EntrypointImageType::Image:
        image = CustomSymbolWithPointSize(
            base::SysUTF8ToNSString(config->entrypoint_image_name),
            symbolPointSize);
        break;
    }

    _badgeIcon.image = image;
  }
}

- (void)setInfobarBadgesCurrentlyShown:(BOOL)infobarBadgesCurrentlyShown {
  _infobarBadgesCurrentlyShown = infobarBadgesCurrentlyShown;
  [self refreshEntrypointVisualElements];
  [self collapseBadgeContainer];
}

- (void)showEntrypoint {
  [self showBadge];
}

- (void)hideEntrypoint {
  if ([_badgeConfig isContextualPanelEntrypointBadge]) {
    [self hideBadge];
  }
}

- (void)transitionToLargeEntrypoint {
  [self expandBadgeContainer];
}

- (void)transitionToSmallEntrypoint {
  [self collapseBadgeContainer];
}

- (void)transitionToContextualPanelOpenedState:(BOOL)opened {
  _badgeTapped = opened;
  [self refreshEntrypointVisualElements];
  [self collapseBadgeContainer];
}

- (void)setEntrypointColored:(BOOL)colored {
  [self highlightBadge:colored];
}

- (void)updateAccessibilityStatus {
  _buttonContainer.isAccessibilityElement = !self.view.hidden;
}

#pragma mark - LocationBarBadgeConsumer

- (void)highlightBadge:(BOOL)highlight {
  if (!ShouldHighlightContextualPanelEntrypointDuringIPH()) {
    return;
  }

  __weak LocationBarBadgeViewController* weakSelf = self;

  [UIView animateWithDuration:kBadgeDisplayingAnimationTime
                        delay:0
                      options:(UIViewAnimationOptionCurveEaseOut |
                               UIViewAnimationOptionAllowUserInteraction)
                   animations:^{
                     [weakSelf updateBadgeHighlight:highlight];
                   }
                   completion:nil];
}

- (void)showBadge {
  if (_locationBarBadgeShouldBeVisible) {
    return;
  }

  if (_badgeConfig.badgeType ==
      LocationBarBadgeType::kGeminiContextualCueChip) {
    if ([self.visibilityDelegate
            respondsToSelector:@selector(disableProactiveSuggestionOverlay:)]) {
      [self.visibilityDelegate disableProactiveSuggestionOverlay:YES];
    }
  }

  [self refreshEntrypointVisualElements];

  _locationBarBadgeShouldBeVisible = YES;

  if (_shouldCollapseForFullscreen) {
    return;
  }

  // Animate the badge appearance.
  self.view.alpha = 0;
  self.view.transform = CGAffineTransformMakeScale(0.95, 0.95);

  [self setLocationBarBadgeHidden:NO];

  [self updateAccessibilityStatus];

  __weak LocationBarBadgeViewController* weakSelf = self;

  [UIView animateWithDuration:kBadgeDisplayingAnimationTime
      delay:0
      options:(UIViewAnimationOptionCurveEaseIn |
               UIViewAnimationOptionAllowUserInteraction)
      animations:^{
        self.view.alpha = 1;
        self.view.transform = CGAffineTransformIdentity;
      }
      completion:^(BOOL completed) {
        [weakSelf refreshVoiceOverBoundingBoxIfFocused];
      }];
}

- (void)hideBadge {
  [self collapseBadgeContainer];
  [self transitionToContextualPanelOpenedState:NO];

  [self setLocationBarBadgeHidden:YES];
  if (_badgeConfig.badgeType ==
      LocationBarBadgeType::kGeminiContextualCueChip) {
    if ([self.visibilityDelegate
            respondsToSelector:@selector(disableProactiveSuggestionOverlay:)]) {
      [self.visibilityDelegate disableProactiveSuggestionOverlay:NO];
    }
  }

  [self updateAccessibilityStatus];
  [self setLocationBarLabelCenteredBetweenContent:NO];

  [self.view layoutIfNeeded];

  [self refreshVoiceOverBoundingBoxIfFocused];
}

- (void)collapseBadgeContainer {
  if (_collapsedContainerTrailingConstraint.active) {
    return;
  }

  __weak LocationBarBadgeViewController* weakSelf = self;

  void (^animateBadgeContainerCollapse)() = ^{
    LocationBarBadgeViewController* strongSelf = weakSelf;

    if (!strongSelf) {
      return;
    }

    [strongSelf activateCollapsedContainerConstraint];
    [strongSelf setLocationBarLabelCenteredBetweenContent:NO];
    [strongSelf.view layoutIfNeeded];
  };

  [UIView animateWithDuration:kBadgeContainerCollapseAnimationTime
                        delay:0
                      options:(UIViewAnimationOptionCurveEaseOut |
                               UIViewAnimationOptionAllowUserInteraction)
                   animations:animateBadgeContainerCollapse
                   completion:^(BOOL completed) {
                     [weakSelf didCollapseBadgeContainer];
                   }];

  [_buttonContainer removeGestureRecognizer:_swipeRecognizer];

  [_layoutGuideCenter referenceView:nil
                          underName:kLocationBarBadgeLargeEntrypointGuide];
}

- (void)expandBadgeContainer {
  if (_expandedContainerTrailingConstraint.active) {
    return;
  }

  [_layoutGuideCenter referenceView:_buttonContainer
                          underName:kLocationBarBadgeLargeEntrypointGuide];

  _swipeRecognizer = [[UISwipeGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(expandedBadgeSwiped)];
  _swipeRecognizer.cancelsTouchesInView = YES;
  _swipeRecognizer.direction = base::i18n::IsRTL()
                                   ? UISwipeGestureRecognizerDirectionRight
                                   : UISwipeGestureRecognizerDirectionLeft;
  [_buttonContainer addGestureRecognizer:_swipeRecognizer];

  __weak LocationBarBadgeViewController* weakSelf = self;

  void (^animateTransitionToLargeEntrypoint)() = ^{
    LocationBarBadgeViewController* strongSelf = weakSelf;

    if (!strongSelf) {
      return;
    }

    [strongSelf activateExpandedContainerConstraint];
    [strongSelf setLocationBarLabelCenteredBetweenContent:YES];
    [strongSelf.view layoutIfNeeded];
  };

  [UIView animateWithDuration:kBadgeContainerExpandAnimationTime
                        delay:0
                      options:(UIViewAnimationOptionCurveEaseOut |
                               UIViewAnimationOptionAllowUserInteraction)
                   animations:animateTransitionToLargeEntrypoint
                   completion:^(BOOL completed) {
                     [weakSelf refreshVoiceOverBoundingBoxIfFocused];
                   }];
}

- (BOOL)isBadgeVisible {
  return _locationBarBadgeShouldBeVisible;
}

- (void)showUnreadBadge:(BOOL)unread {
  if (!_unreadIndicatorView && unread) {
    // Add unread indicator to the displayed badge.
    _unreadIndicatorView = [[UIView alloc] init];
    _unreadIndicatorView.layer.cornerRadius = kUnreadIndicatorViewHeight / 2;
    _unreadIndicatorView.backgroundColor = [UIColor colorNamed:kBlueColor];
    _unreadIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
    _unreadIndicatorView.accessibilityIdentifier =
        kBadgeUnreadIndicatorAccessibilityIdentifier;

    // TODO(crbug.com/457567241): Add _unreadIndicatorView as a subview and
    // related constraints.
    _unreadIndicatorView.hidden = !unread;
  }
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  _shouldCollapseForFullscreen = progress <= kFullscreenProgressThreshold;
  if (_shouldCollapseForFullscreen) {
    _buttonContainer.hidden = YES;
  } else {
    // Fade in/out the badge.
    CGFloat alphaValue = fmax((progress - kFullscreenProgressThreshold) /
                                  (1 - kFullscreenProgressThreshold),
                              0);
    _buttonContainer.alpha = alphaValue;
    _buttonContainer.hidden = NO;
  }

  [self updateAccessibilityStatus];
}

@end
