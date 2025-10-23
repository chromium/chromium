// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_view_controller.h"

#import "base/i18n/rtl.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_util.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_mutator.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"
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
  // Whether the location bar badge should be visible.
  BOOL _locationBarBadgeShouldBeVisible;

  // The UIButton contains a UIView, which itself contains the
  // badge's image and label. The container (UIButton) is needed for
  // button-like behavior and to create the shadow around the entire entrypoint
  // package. The content UIView is needed to populate the inner contents of
  // the button and to clip the label to the badge's bounds for proper
  // animations and sizing.
  UIButton* _buttonContainer;
  UIView* _badgeContentView;
  UIImageView* _badgeIcon;
  UILabel* _label;

  // The small vertical pill-shaped line separating the Location Bar Badge
  // entrypoint and Infobar badges, if present.
  UIView* _separator;

  // Constraints for the two states of the trailing edge of the entrypoint
  // container. They are activated/deactivated as needed when the label is
  // shown/hidden.
  NSLayoutConstraint* _largeTrailingConstraint;
  NSLayoutConstraint* _smallTrailingConstraint;

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
}

#pragma mark - Public

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set the view as hidden when created as it should only appear when the
  // entrypoint should be shown.
  self.view.hidden = YES;
  self.view.isAccessibilityElement = NO;
  _locationBarBadgeShouldBeVisible = NO;

  _buttonContainer = [self configuredButtonContainer];
  _badgeContentView = [self configuredBadgeContentView];
  _badgeIcon = [self configuredBadgeIcon];
  _label = [self configuredLabel];
  _separator = [self configuredSeparator];

  [self.view addSubview:_buttonContainer];
  [self.view addSubview:_separator];
  [_buttonContainer addSubview:_badgeContentView];
  [_badgeContentView addSubview:_badgeIcon];
  [_badgeContentView addSubview:_label];

  _buttonContainer.isAccessibilityElement = !self.view.hidden;

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
  [self setLocationBarBadgeHidden:hidden];
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

// Returns the button configuration with the given background color.
- (UIButtonConfiguration*)buttonConfigurationWithBackgroundColor:
    (UIColor*)backgroundColor {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration filledButtonConfiguration];
  configuration.baseBackgroundColor = backgroundColor;
  configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  return configuration;
}

// Creates and configures the button container.
- (UIButton*)configuredButtonContainer {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  UIColor* defaultBackgroundColor = [UIColor colorNamed:kBackgroundColor];
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
                action:@selector(userTappedEntrypoint)
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

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kBadgeSymbolPointSize
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

  _smallTrailingConstraint = [_buttonContainer.trailingAnchor
      constraintEqualToAnchor:_badgeIcon.trailingAnchor];
  _largeTrailingConstraint = [_buttonContainer.trailingAnchor
      constraintEqualToAnchor:labelTrailingSpace.trailingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [self.view.widthAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.heightAnchor],
    _smallTrailingConstraint,
    // The entrypoint doesn't fully fill the height of the location bar, so to
    // make it exactly follow the curvature of the location bar's corner radius,
    // it must be placed with the same amount of margin space horizontally that
    // exists vertically between the entrypoint and the location bar itself.
    [leadingSpace.widthAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:((1 - kBadgeHeightMultiplier) / 2)],
    [leadingSpace.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [leadingSpace.trailingAnchor
        constraintEqualToAnchor:_buttonContainer.leadingAnchor],
    [_buttonContainer.leadingAnchor
        constraintEqualToAnchor:leadingSpace.trailingAnchor],
    [_separator.centerXAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [_separator.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
    [_separator.widthAnchor constraintEqualToConstant:kSeparatorWidthConstant],
    [_separator.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kSeparatorHeightMultiplier],
    [_buttonContainer.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kBadgeHeightMultiplier],
    [_buttonContainer.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [self.view.leadingAnchor
        constraintEqualToAnchor:leadingSpace.leadingAnchor],
    [self.view.trailingAnchor
        constraintGreaterThanOrEqualToAnchor:_buttonContainer.trailingAnchor],
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

- (void)activateLargeEntrypointTrailingConstraint {
  _smallTrailingConstraint.active = NO;
  _largeTrailingConstraint.active = YES;
}

- (void)activateSmallEntrypointTrailingConstraint {
  _largeTrailingConstraint.active = NO;
  _smallTrailingConstraint.active = YES;
}

- (void)updateLabelFont {
  _label.font = [self badgeLabelFont];
}

- (void)dismissIPHWithoutAnimation {
  [self.mutator dismissIPHAnimated:NO];
}

// Returns the preferred font and size given the current ContentSizeCategory.
- (UIFont*)badgeLabelFont {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleFootnote,
      self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityLarge);
}

// Refreshes the VoiceOver bounding box and notifies the mutator that the
// animation to transition to a small entrypoint has completed.
- (void)didCompleteTransitionToSmallEntrypoint {
  [self refreshVoiceOverBoundingBoxIfFocused];
  [self.mutator didCompleteTransitionToSmallEntrypoint];
}

// Sets the proper visual features depending on current infobar badges status
// and whether the Location Bar Badge is open.
- (void)refreshEntrypointVisualElements {
  BOOL shouldAccountForVisibleInfobarBadges =
      _infobarBadgesCurrentlyShown && !IsReaderModeAvailable();
  BOOL shouldShowMutedColors =
      shouldAccountForVisibleInfobarBadges || _badgeTapped;

  // Badge icon tint color.
  _badgeIcon.tintColor = shouldShowMutedColors
                             ? [UIColor colorNamed:kGrey600Color]
                             : [UIColor colorNamed:kBlue600Color];

  // Button container shadow.
  _buttonContainer.layer.shadowOpacity =
      shouldShowMutedColors ? 0 : kBadgeContainerShadowOpacity;

  // Button container background color.
  UIColor* untappedBackgroundColor =
      shouldAccountForVisibleInfobarBadges
          ? nil
          : [UIColor colorNamed:kBackgroundColor];

  UIColor* buttonContainerBackgroundColor =
      _badgeTapped ? [UIColor colorNamed:kGrey100Color]
                   : untappedBackgroundColor;
  _buttonContainer.configuration = [self
      buttonConfigurationWithBackgroundColor:buttonContainerBackgroundColor];

  // Separator visibility.
  _separator.hidden = !_infobarBadgesCurrentlyShown;
}

// Applies the correct color to the entrypoint (highlighted blue when the
// in-product help is present), otherwise back to the normal colorset.
- (void)styleEntrypointForColoredState:(BOOL)colored {
  _badgeIcon.tintColor = colored ? [UIColor colorNamed:kBackgroundColor]
                                 : [UIColor colorNamed:kBlue600Color];

  // Update entrypoint container background.
  UIColor* buttonContainerBackgroundColor =
      colored ? [UIColor colorNamed:kBlue600Color]
              : [UIColor colorNamed:kBackgroundColor];
  _buttonContainer.configuration = [self
      buttonConfigurationWithBackgroundColor:buttonContainerBackgroundColor];
}

// User swiped the large entrypoint chip towards the leading edge, intending to
// dismiss it.
- (void)largeEntrypointChipSwiped {
  [self transitionToSmallEntrypoint];
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

// Notify that a user tapped the entrypoint.
- (void)userTappedEntrypoint {
  _badgeTapped = YES;
  [self refreshEntrypointVisualElements];
  [self transitionToSmallEntrypoint];
  [self.mutator entrypointTapped];
}

- (void)setLocationBarBadgeHidden:(BOOL)hidden {
  _locationBarBadgeShouldBeVisible = !hidden;
  self.view.hidden = hidden;
  [self updateViewsVisibility];
  // TODO(crbug.com/429140788): Remove after migration and BadgesContainerView
  // is obsolete.
  [self.visibilityDelegate setContextualPanelEntrypointHidden:hidden];
}

#pragma mark - ContextualPanelEntrypointConsumer

- (void)setEntrypointConfig:(ContextualPanelItemConfiguration*)config {
  if (IsAskGeminiChipEnabled()) {
    NSString* accessibilityLabel =
        base::SysUTF8ToNSString(config->accessibility_label);

    UIImage* image;
    switch (config->image_type) {
      case ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol:
        image = DefaultSymbolWithPointSize(
            base::SysUTF8ToNSString(config->entrypoint_image_name),
            kBadgeSymbolPointSize);
        break;
      case ContextualPanelItemConfiguration::EntrypointImageType::Image:
        image = CustomSymbolWithPointSize(
            base::SysUTF8ToNSString(config->entrypoint_image_name),
            kBadgeSymbolPointSize);
        break;
    }

    LocationBarBadgeConfiguration* badgeConfig =
        [[LocationBarBadgeConfiguration alloc]
            initWithAccessibilityLabel:accessibilityLabel
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
    switch (config->image_type) {
      case ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol:
        image = DefaultSymbolWithPointSize(
            base::SysUTF8ToNSString(config->entrypoint_image_name),
            kBadgeSymbolPointSize);
        break;
      case ContextualPanelItemConfiguration::EntrypointImageType::Image:
        image = CustomSymbolWithPointSize(
            base::SysUTF8ToNSString(config->entrypoint_image_name),
            kBadgeSymbolPointSize);
        break;
    }

    _badgeIcon.image = image;
  }
}

- (void)setInfobarBadgesCurrentlyShown:(BOOL)infobarBadgesCurrentlyShown {
  _infobarBadgesCurrentlyShown = infobarBadgesCurrentlyShown;
  [self refreshEntrypointVisualElements];
  [self transitionToSmallEntrypoint];
}

- (void)showEntrypoint {
  [self refreshEntrypointVisualElements];

  if (_locationBarBadgeShouldBeVisible) {
    return;
  }

  _locationBarBadgeShouldBeVisible = YES;

  if (_shouldCollapseForFullscreen) {
    return;
  }

  // Animate the entrypoint appearance.
  self.view.alpha = 0;
  self.view.transform = CGAffineTransformMakeScale(0.95, 0.95);

  [self setContextualPanelEntrypointHidden:NO];

  _buttonContainer.isAccessibilityElement = !self.view.hidden;

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

- (void)hideEntrypoint {
  [self transitionToSmallEntrypoint];
  [self transitionToContextualPanelOpenedState:NO];

  _locationBarBadgeShouldBeVisible = NO;
  [self setContextualPanelEntrypointHidden:YES];

  _buttonContainer.isAccessibilityElement = !self.view.hidden;

  [self.mutator setLocationBarLabelCenteredBetweenContent:NO];

  [self.view layoutIfNeeded];

  [self refreshVoiceOverBoundingBoxIfFocused];
}

- (void)transitionToLargeEntrypoint {
  if (_largeTrailingConstraint.active) {
    return;
  }

  [_layoutGuideCenter referenceView:_buttonContainer
                          underName:kLocationBarBadgeLargeEntrypointGuide];

  _swipeRecognizer = [[UISwipeGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(largeEntrypointChipSwiped)];
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

    [strongSelf activateLargeEntrypointTrailingConstraint];
    [strongSelf.mutator setLocationBarLabelCenteredBetweenContent:YES];
    [strongSelf.view layoutIfNeeded];
  };

  [UIView animateWithDuration:kLargeBadgeAppearingAnimationTime
                        delay:0
                      options:(UIViewAnimationOptionCurveEaseOut |
                               UIViewAnimationOptionAllowUserInteraction)
                   animations:animateTransitionToLargeEntrypoint
                   completion:^(BOOL completed) {
                     [weakSelf refreshVoiceOverBoundingBoxIfFocused];
                   }];
}

- (void)transitionToSmallEntrypoint {
  if (_smallTrailingConstraint.active) {
    return;
  }

  __weak LocationBarBadgeViewController* weakSelf = self;

  void (^animateTransitionToSmallEntrypoint)() = ^{
    LocationBarBadgeViewController* strongSelf = weakSelf;

    if (!strongSelf) {
      return;
    }

    [strongSelf activateSmallEntrypointTrailingConstraint];
    [strongSelf.mutator setLocationBarLabelCenteredBetweenContent:NO];
    [strongSelf.view layoutIfNeeded];
  };

  [UIView animateWithDuration:kLargeBadgeDisappearingAnimationTime
                        delay:0
                      options:(UIViewAnimationOptionCurveEaseOut |
                               UIViewAnimationOptionAllowUserInteraction)
                   animations:animateTransitionToSmallEntrypoint
                   completion:^(BOOL completed) {
                     [weakSelf didCompleteTransitionToSmallEntrypoint];
                   }];

  [_buttonContainer removeGestureRecognizer:_swipeRecognizer];

  [_layoutGuideCenter referenceView:nil
                          underName:kLocationBarBadgeLargeEntrypointGuide];
}

- (void)transitionToContextualPanelOpenedState:(BOOL)opened {
  _badgeTapped = opened;
  [self refreshEntrypointVisualElements];
  [self transitionToSmallEntrypoint];
}

- (void)setEntrypointColored:(BOOL)colored {
  if (!ShouldHighlightContextualPanelEntrypointDuringIPH()) {
    return;
  }

  __weak LocationBarBadgeViewController* weakSelf = self;

  [UIView animateWithDuration:kBadgeDisplayingAnimationTime
                        delay:0
                      options:(UIViewAnimationOptionCurveEaseOut |
                               UIViewAnimationOptionAllowUserInteraction)
                   animations:^{
                     [weakSelf styleEntrypointForColoredState:colored];
                   }
                   completion:nil];
}

#pragma mark FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  _shouldCollapseForFullscreen = progress <= kFullscreenProgressThreshold;
  if (_shouldCollapseForFullscreen) {
    [self setContextualPanelEntrypointHidden:YES];
  } else {
    [self setContextualPanelEntrypointHidden:!_locationBarBadgeShouldBeVisible];

    // Fade in/out the entrypoint badge.
    CGFloat alphaValue = fmax((progress - kFullscreenProgressThreshold) /
                                  (1 - kFullscreenProgressThreshold),
                              0);
    self.view.alpha = alphaValue;
  }

  _buttonContainer.isAccessibilityElement = !self.view.hidden;
}

@end
