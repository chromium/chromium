// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/ui_bundled/suspicious_site_warning_view_controller.h"

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Card dimensions and corner radii.
constexpr CGFloat kAlertWidth = 312;
constexpr CGFloat kLegacyAlertWidth = 270;
constexpr CGFloat kAlertWidthAccessibility = 402;
constexpr CGFloat kCornerRadius = 34;
constexpr CGFloat kLegacyCornerRadius = 14;
constexpr CGFloat kMinimumEdgeMargin = 16;

// Legacy card shadow properties (pre-iOS 26).
constexpr CGFloat kShadowOffsetX = 0;
constexpr CGFloat kShadowOffsetY = 15;
constexpr CGFloat kShadowRadius = 13;
constexpr float kShadowOpacity = 0.12;

// Backdrop dimming scrim alphas (iOS 26+ matches system alert dimming).
constexpr CGFloat kLightModeScrimAlpha = 0.2;
constexpr CGFloat kDarkModeScrimAlpha = 0.4;

// Content padding and vertical spacing.
constexpr CGFloat kAlertMarginTop = 24;
constexpr CGFloat kAlertMarginBottom = 16;
constexpr CGFloat kTextHorizontalInset = 24;
constexpr CGFloat kLegacyTextHorizontalInset = 20;
constexpr CGFloat kTitleToMessageSpacing = 8;
constexpr CGFloat kMessageToActionsSpacing = 22;
constexpr CGFloat kLegacyMessageToActionsSpacing = 16;

// Action button geometry and visual styling.
constexpr CGFloat kButtonHorizontalMargin = 16;
constexpr CGFloat kButtonVerticalContentInset = 14;
constexpr CGFloat kButtonHorizontalContentInset = 16;
constexpr CGFloat kButtonCornerRadius = 24;
constexpr CGFloat kButtonSpacing = 10;
constexpr CGFloat kButtonHighlightedAlpha = 0.8;

// Returns the target alert width for the current OS version and
// `contentSizeCategory`. iOS 26's Liquid Glass redesign widened system alerts
// from the classic 270pt pre-iOS 26 width to accommodate larger corner radii,
// wider insets, and capsule buttons.
CGFloat GetAlertWidth(UIContentSizeCategory contentSizeCategory) {
  if (UIContentSizeCategoryIsAccessibilityCategory(contentSizeCategory)) {
    return kAlertWidthAccessibility;
  }
  if (@available(iOS 26, *)) {
    return kAlertWidth;
  }
  return kLegacyAlertWidth;
}

// Returns the backdrop dimming scrim color.
UIColor* GetScrimColor() {
  if (@available(iOS 26, *)) {
    return [UIColor
        colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
          CGFloat alpha =
              traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
                  ? kDarkModeScrimAlpha
                  : kLightModeScrimAlpha;
          return [UIColor colorWithWhite:0 alpha:alpha];
        }];
  }
  return [UIColor colorNamed:kScrimBackgroundColor];
}

// Returns the background color for an action button based on its `isPrimary`
// role and current `state`.
UIColor* GetButtonBackgroundColor(BOOL isPrimary, UIControlState state) {
  BOOL isHighlighted =
      (state & (UIControlStateHighlighted | UIControlStateFocused |
                UIControlStateSelected)) != 0;
  UIColor* baseColor =
      [UIColor colorNamed:isPrimary ? kBlueColor : kTertiaryBackgroundColor];
  return isHighlighted
             ? [baseColor colorWithAlphaComponent:kButtonHighlightedAlpha]
             : baseColor;
}

// Creates a capsule action button styled as either the primary ("Back to
// safety") or secondary ("Mark as safe") action.
UIButton* CreateActionButton(NSString* title, BOOL isPrimary) {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.title = title;
  configuration.titleAlignment = UIButtonConfigurationTitleAlignmentCenter;
  configuration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalContentInset, kButtonHorizontalContentInset,
      kButtonVerticalContentInset, kButtonHorizontalContentInset);
  configuration.background.cornerRadius = kButtonCornerRadius;
  configuration.background.backgroundColor =
      GetButtonBackgroundColor(isPrimary, UIControlStateNormal);
  configuration.baseForegroundColor =
      isPrimary ? [UIColor colorNamed:kSolidButtonTextColor]
                : [UIColor colorNamed:kTextPrimaryColor];

  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

  __weak UIButton* weakButton = button;
  configuration = button.configuration;
  configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [incoming mutableCopy];
    outgoing[NSFontAttributeName] =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline
            compatibleWithTraitCollection:weakButton.traitCollection];
    return outgoing;
  };
  button.configuration = configuration;

  button.configurationUpdateHandler = ^(UIButton* updatedButton) {
    UIButtonConfiguration* updatedConfig = updatedButton.configuration;
    CHECK(updatedConfig);
    UIColor* backgroundColor =
        GetButtonBackgroundColor(isPrimary, updatedButton.state);
    if (![updatedConfig.background.backgroundColor isEqual:backgroundColor]) {
      updatedConfig = [updatedConfig copy];
      updatedConfig.background.backgroundColor = backgroundColor;
      updatedButton.configuration = updatedConfig;
    }
  };
  return button;
}

}  // namespace

@implementation SuspiciousSiteWarningViewController {
  // The content container inside the alert card.
  UIView* _contentView;
  // Width constraint for the card, updated when Dynamic Type changes.
  NSLayoutConstraint* _contentViewWidthConstraint;
  // Action buttons retained to prevent duplicate taps once an action is chosen.
  UIButton* _backToSafetyButton;
  UIButton* _markAsSafeButton;
  // YES once the user has tapped one of the action buttons.
  BOOL _actionSelected;
}

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    self.modalPresentationStyle = UIModalPresentationOverFullScreen;
    self.modalTransitionStyle = UIModalTransitionStyleCrossDissolve;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = GetScrimColor();
  self.view.accessibilityViewIsModal = YES;

  [self configureContentView];
  [self configureLayout];

  [self
      registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                   withAction:@selector(preferredContentSizeCategoryDidChange)];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if (self.isBeingDismissed && !_actionSelected) {
    [self.delegate
        suspiciousSiteWarningViewControllerDidDismissWithoutAction:self];
  }
}

#pragma mark - Private

// Creates and positions the Liquid Glass (iOS 26+) or legacy alert container.
- (void)configureContentView {
  UIView* cardView = nil;
  if (@available(iOS 26, *)) {
    UIGlassEffect* glassEffect = [[UIGlassEffect alloc] init];
    glassEffect.interactive = NO;
    UIVisualEffectView* backgroundView =
        [[UIVisualEffectView alloc] initWithEffect:glassEffect];
    backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    backgroundView.cornerConfiguration = [UICornerConfiguration
        capsuleConfigurationWithMaximumRadius:kCornerRadius];
    [self.view addSubview:backgroundView];
    _contentView = backgroundView.contentView;
    _contentView.translatesAutoresizingMaskIntoConstraints = NO;
    _contentView.clipsToBounds = YES;
    AddSameConstraints(_contentView, backgroundView);
    cardView = backgroundView;
  } else {
    // Separate the outer shadow-casting container from the inner clipped
    // container so scrolled content respects the rounded corners without
    // clipping the drop shadow.
    UIView* shadowView = [[UIView alloc] init];
    shadowView.translatesAutoresizingMaskIntoConstraints = NO;
    shadowView.layer.shadowOffset = CGSizeMake(kShadowOffsetX, kShadowOffsetY);
    shadowView.layer.shadowRadius = kShadowRadius;
    shadowView.layer.shadowOpacity = kShadowOpacity;
    [self.view addSubview:shadowView];

    UIView* clippedView = [[UIView alloc] init];
    clippedView.translatesAutoresizingMaskIntoConstraints = NO;
    clippedView.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];
    clippedView.layer.cornerRadius = kLegacyCornerRadius;
    clippedView.clipsToBounds = YES;
    [shadowView addSubview:clippedView];
    AddSameConstraints(clippedView, shadowView);

    _contentView = clippedView;
    cardView = shadowView;
  }

  _contentViewWidthConstraint = [cardView.widthAnchor
      constraintEqualToConstant:GetAlertWidth(
                                    self.traitCollection
                                        .preferredContentSizeCategory)];
  _contentViewWidthConstraint.priority = UILayoutPriorityRequired - 1;

  UILayoutGuide* safeArea = self.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    _contentViewWidthConstraint,
    [cardView.centerXAnchor constraintEqualToAnchor:safeArea.centerXAnchor],
    [cardView.centerYAnchor constraintEqualToAnchor:safeArea.centerYAnchor],
    [cardView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:safeArea.leadingAnchor
                                    constant:kMinimumEdgeMargin],
    [cardView.trailingAnchor
        constraintLessThanOrEqualToAnchor:safeArea.trailingAnchor
                                 constant:-kMinimumEdgeMargin],
    [cardView.topAnchor
        constraintGreaterThanOrEqualToAnchor:safeArea.topAnchor
                                    constant:kMinimumEdgeMargin],
    [cardView.bottomAnchor
        constraintLessThanOrEqualToAnchor:safeArea.bottomAnchor
                                 constant:-kMinimumEdgeMargin],
  ]];
}

// Builds the scrollable content stack containing the title, message, and
// vertically stacked action buttons.
- (void)configureLayout {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.delaysContentTouches = NO;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  if (@available(iOS 26, *)) {
    scrollView.verticalScrollIndicatorInsets =
        UIEdgeInsetsMake(kCornerRadius, 0, kCornerRadius, 0);
  }
  [_contentView addSubview:scrollView];
  AddSameConstraints(scrollView, _contentView);

  NSLayoutConstraint* heightConstraint = [scrollView.heightAnchor
      constraintEqualToAnchor:scrollView.contentLayoutGuide.heightAnchor];
  // Allow the scroll view to shrink and scroll when Dynamic Type or landscape
  // height constraints prevent showing the full alert at once.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;

  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.alignment = UIStackViewAlignmentCenter;
  [scrollView addSubview:stackView];
  AddSameConstraintsWithInsets(
      stackView, scrollView,
      NSDirectionalEdgeInsetsMake(kAlertMarginTop, 0, kAlertMarginBottom, 0));

  CGFloat textHorizontalInset = kLegacyTextHorizontalInset;
  CGFloat messageToActionsSpacing = kLegacyMessageToActionsSpacing;
  if (@available(iOS 26, *)) {
    textHorizontalInset = kTextHorizontalInset;
    messageToActionsSpacing = kMessageToActionsSpacing;
  }
  NSDirectionalEdgeInsets textInsets = NSDirectionalEdgeInsetsMake(
      0, textHorizontalInset, 0, textHorizontalInset);

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.numberOfLines = 0;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  if (@available(iOS 26, *)) {
    titleLabel.textAlignment = NSTextAlignmentNatural;
  }
  titleLabel.text = l10n_util::GetNSString(IDS_SUSPICIOUS_SITE_TITLE);
  [stackView addArrangedSubview:titleLabel];
  [stackView setCustomSpacing:kTitleToMessageSpacing afterView:titleLabel];
  AddSameConstraintsToSidesWithInsets(titleLabel, _contentView,
                                      LayoutSides::kHorizontal, textInsets);

  UILabel* messageLabel = [[UILabel alloc] init];
  messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
  messageLabel.numberOfLines = 0;
  messageLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  messageLabel.adjustsFontForContentSizeCategory = YES;
  messageLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  messageLabel.textAlignment = NSTextAlignmentCenter;
  if (@available(iOS 26, *)) {
    messageLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    messageLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    messageLabel.textAlignment = NSTextAlignmentNatural;
  }
  messageLabel.text = l10n_util::GetNSString(IDS_SUSPICIOUS_SITE_MESSAGE);
  [stackView addArrangedSubview:messageLabel];
  [stackView setCustomSpacing:messageToActionsSpacing afterView:messageLabel];
  AddSameConstraintsToSidesWithInsets(messageLabel, _contentView,
                                      LayoutSides::kHorizontal, textInsets);

  _backToSafetyButton = CreateActionButton(
      l10n_util::GetNSString(IDS_SUSPICIOUS_SITE_BACK_TO_SAFETY),
      /*isPrimary=*/YES);
  [_backToSafetyButton addTarget:self
                          action:@selector(didTapBackToSafety)
                forControlEvents:UIControlEventTouchUpInside];

  _markAsSafeButton = CreateActionButton(
      l10n_util::GetNSString(IDS_SUSPICIOUS_SITE_PROCEED_BUTTON),
      /*isPrimary=*/NO);
  [_markAsSafeButton addTarget:self
                        action:@selector(didTapMarkAsSafe)
              forControlEvents:UIControlEventTouchUpInside];

  UIStackView* buttonStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _backToSafetyButton, _markAsSafeButton ]];
  buttonStackView.translatesAutoresizingMaskIntoConstraints = NO;
  buttonStackView.axis = UILayoutConstraintAxisVertical;
  buttonStackView.alignment = UIStackViewAlignmentFill;
  buttonStackView.spacing = kButtonSpacing;
  [stackView addArrangedSubview:buttonStackView];
  AddSameConstraintsToSidesWithInsets(
      buttonStackView, _contentView, LayoutSides::kHorizontal,
      NSDirectionalEdgeInsetsMake(0, kButtonHorizontalMargin, 0,
                                  kButtonHorizontalMargin));
}

// Updates the alert width when the user's Dynamic Type category changes.
- (void)preferredContentSizeCategoryDidChange {
  _contentViewWidthConstraint.constant =
      GetAlertWidth(self.traitCollection.preferredContentSizeCategory);
}

// Disables both action buttons to prevent duplicate invocations while the alert
// is dismissing.
- (void)disableActionButtons {
  _actionSelected = YES;
  _backToSafetyButton.enabled = NO;
  _markAsSafeButton.enabled = NO;
}

// Handles taps on the "Back to safety" button.
- (void)didTapBackToSafety {
  if (_actionSelected) {
    return;
  }
  [self disableActionButtons];
  [self.delegate suspiciousSiteWarningViewControllerDidTapBackToSafety:self];
}

// Handles taps on the "Mark as safe" button.
- (void)didTapMarkAsSafe {
  if (_actionSelected) {
    return;
  }
  [self disableActionButtons];
  [self.delegate suspiciousSiteWarningViewControllerDidTapMarkAsSafe:self];
}

@end
