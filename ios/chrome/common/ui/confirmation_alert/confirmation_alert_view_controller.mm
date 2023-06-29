// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

#import "base/check.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kActionsBottomMargin = 10;
// Gradient height.
const CGFloat kGradientHeight = 40.;
const CGFloat kScrollViewBottomInsets = 20;
const CGFloat kStackViewSpacing = 8;
const CGFloat kStackViewSpacingAfterIllustration = 27;
// The multiplier used when in regular horizontal size class.
const CGFloat kSafeAreaMultiplier = 0.65;
const CGFloat kContentOptimalWidth = 327;

// The size of the symbol image.
const CGFloat kSymbolBadgeImagePointSize = 13;

// The name of the checkmark symbol in filled circle.
NSString* const kCheckmarkSymbol = @"checkmark.circle.fill";

// Properties of the favicon.
const CGFloat kFaviconCornerRadius = 13;
const CGFloat kFaviconShadowOffsetX = 0;
const CGFloat kFaviconShadowOffsetY = 0;
const CGFloat kFaviconShadowRadius = 6;
const CGFloat kFaviconShadowOpacity = 0.1;

// Length of each side of the favicon frame (which contains the favicon and the
// surrounding whitespace).
const CGFloat kFaviconFrameSideLength = 60;

// Length of each side of the favicon.
const CGFloat kFaviconSideLength = 30;

// Length of each side of the favicon badge.
const CGFloat kFaviconBadgeSideLength = 24;

}  // namespace

@interface ConfirmationAlertViewController ()

// References to the UI properties that need to be updated when the trait
// collection changes.
@property(nonatomic, strong) UIButton* primaryActionButton;
@property(nonatomic, strong) UIButton* secondaryActionButton;
@property(nonatomic, strong) UIButton* tertiaryActionButton;
@property(nonatomic, strong) UINavigationBar* navigationBar;
@property(nonatomic, strong) UIImageView* imageView;
@property(nonatomic, strong) UIView* imageContainerView;
@property(nonatomic, strong) NSLayoutConstraint* imageViewAspectRatioConstraint;
@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) GradientView* gradientView;
@property(nonatomic, assign) CGFloat customGradientViewHeight;
@property(nonatomic, strong) NSLayoutConstraint* gradientViewHeightConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* scrollViewBottomAnchorConstraint;
@end

@implementation ConfirmationAlertViewController

#pragma mark - Public

- (UISheetPresentationControllerDetent*)preferredHeightDetent {
  __typeof(self) __weak weakSelf = self;
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf detentForPreferredHeightInContext:context];
  };
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:@"preferred_height"
                        resolver:resolver];
}

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _customSpacingAfterImage = kStackViewSpacingAfterIllustration;
    _customGradientViewHeight = kGradientHeight;
    _customScrollViewBottomInsets = kScrollViewBottomInsets;
    _customSpacing = kStackViewSpacing;
    _showsVerticalScrollIndicator = YES;
    _scrollEnabled = YES;
    _showDismissBarButton = YES;
    _dismissBarButtonSystemItem = UIBarButtonSystemItemDone;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  if (self.hasNavigationBar) {
    self.navigationBar = [self createNavigationBar];
    [self.view addSubview:self.navigationBar];
  }

  if (self.image) {
    if (self.imageEnclosedWithShadowAndBadge ||
        self.imageEnclosedWithShadowWithoutBadge) {
      // The image view is set within the helper method.
      self.imageContainerView =
          [self createImageContainerViewWithShadowAndBadge];
    } else {
      // The image container and the image view are the same.
      self.imageView = [self createImageView];
      self.imageContainerView = self.imageView;
    }
  }

  NSMutableArray* stackSubviews = [[NSMutableArray alloc] init];

  if (self.image) {
    [stackSubviews addObject:self.imageContainerView];
  }

  if (self.titleString.length) {
    UILabel* title = [self createTitleLabel];
    [stackSubviews addObject:title];
  }

  if (self.secondaryTitleString.length) {
    UITextView* secondaryTitle = [self createSecondaryTitleView];
    [stackSubviews addObject:secondaryTitle];
  }

  if (self.subtitleString.length) {
    UITextView* subtitle = [self createSubtitleView];
    [stackSubviews addObject:subtitle];
  }

  if (self.underTitleView) {
    [stackSubviews addObject:self.underTitleView];
  }

  DCHECK(stackSubviews);

  UIStackView* stackView =
      [self createStackViewWithArrangedSubviews:stackSubviews];

  self.scrollView = [self createScrollView];
  [self.scrollView addSubview:stackView];
  [self.view addSubview:self.scrollView];

  self.view.preservesSuperviewLayoutMargins = YES;
  UILayoutGuide* margins = self.view.layoutMarginsGuide;

  if (self.hasNavigationBar) {
    // Constraints the navigation bar to the top.
    AddSameConstraintsToSides(
        self.navigationBar, self.view.safeAreaLayoutGuide,
        LayoutSides::kTrailing | LayoutSides::kTop | LayoutSides::kLeading);
  }

  // Constraint top/bottom of the stack view to the scroll view. This defines
  // the content area. No need to contraint horizontally as we don't want
  // horizontal scroll.
  [NSLayoutConstraint activateConstraints:@[
    [stackView.topAnchor constraintEqualToAnchor:self.scrollView.topAnchor],
    [stackView.bottomAnchor
        constraintEqualToAnchor:self.scrollView.bottomAnchor
                       constant:-self.customScrollViewBottomInsets]
  ]];

  // Scroll View constraints to the height of its content. This allows to center
  // the scroll view.
  NSLayoutConstraint* heightConstraint = [self.scrollView.heightAnchor
      constraintEqualToAnchor:self.scrollView.contentLayoutGuide.heightAnchor];
  // UILayoutPriorityDefaultHigh is the default priority for content
  // compression. Setting this lower avoids compressing the content of the
  // scroll view.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;

  [NSLayoutConstraint activateConstraints:@[
    [stackView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    // Width Scroll View constraint for regular mode.
    [stackView.widthAnchor
        constraintGreaterThanOrEqualToAnchor:margins.widthAnchor
                                  multiplier:kSafeAreaMultiplier],
    // Disable horizontal scrolling.
    [stackView.widthAnchor
        constraintLessThanOrEqualToAnchor:margins.widthAnchor],
  ]];

  // This constraint is added to enforce that the content width should be as
  // close to the optimal width as possible, within the range already activated
  // for "stackView.widthAnchor" previously, with a higher priority.
  NSLayoutConstraint* contentLayoutGuideWidthConstraint =
      [stackView.widthAnchor constraintEqualToConstant:kContentOptimalWidth];
  contentLayoutGuideWidthConstraint.priority = UILayoutPriorityRequired - 1;
  contentLayoutGuideWidthConstraint.active = YES;

  // The bottom anchor for the scroll view.
  NSLayoutYAxisAnchor* scrollViewBottomAnchor =
      self.view.safeAreaLayoutGuide.bottomAnchor;

  BOOL hasActionButton = self.primaryActionString ||
                         self.secondaryActionString ||
                         self.tertiaryActionString;
  if (hasActionButton) {
    UIView* actionStackView = [self createActionStackView];
    [self.view addSubview:actionStackView];

    // Add a low priority width constraints to make sure that the buttons are
    // taking as much width as they can.
    CGFloat extraBottomMargin =
        self.secondaryActionString ? 0 : kActionsBottomMargin;
    NSLayoutConstraint* lowPriorityWidthConstraint =
        [actionStackView.widthAnchor
            constraintEqualToConstant:kContentOptimalWidth];
    lowPriorityWidthConstraint.priority = UILayoutPriorityDefaultHigh + 1;
    // Also constrain the bottom of the action stack view to the bottom of the
    // safe area, but with a lower priority, so that the action stack view is
    // put as close to the bottom as possible.
    NSLayoutConstraint* actionBottomConstraint = [actionStackView.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
    actionBottomConstraint.priority = UILayoutPriorityDefaultLow;
    actionBottomConstraint.active = YES;

    [NSLayoutConstraint activateConstraints:@[
      [actionStackView.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.scrollView.leadingAnchor],
      [actionStackView.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.scrollView.trailingAnchor],
      [actionStackView.centerXAnchor
          constraintEqualToAnchor:self.view.centerXAnchor],
      [actionStackView.widthAnchor
          constraintEqualToAnchor:stackView.widthAnchor],
      [actionStackView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                   constant:-kActionsBottomMargin -
                                            extraBottomMargin],
      [actionStackView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                .bottomAnchor
                                   constant:-extraBottomMargin],
      lowPriorityWidthConstraint
    ]];
    scrollViewBottomAnchor = actionStackView.topAnchor;

    self.gradientView = [self createGradientView];
    [self.view addSubview:self.gradientView];

    [NSLayoutConstraint activateConstraints:@[
      [self.gradientView.bottomAnchor
          constraintEqualToAnchor:actionStackView.topAnchor],
      [self.gradientView.leadingAnchor
          constraintEqualToAnchor:self.scrollView.leadingAnchor],
      [self.gradientView.trailingAnchor
          constraintEqualToAnchor:self.scrollView.trailingAnchor],
    ]];
    self.gradientViewHeightConstraint = [self.gradientView.heightAnchor
        constraintEqualToConstant:self.customGradientViewHeight];
    self.gradientViewHeightConstraint.active = YES;
  }

  self.scrollViewBottomAnchorConstraint = [self.scrollView.bottomAnchor
      constraintLessThanOrEqualToAnchor:scrollViewBottomAnchor
                               constant:-kScrollViewBottomInsets];
  self.scrollViewBottomAnchorConstraint.active = YES;

  [NSLayoutConstraint activateConstraints:@[
    [self.scrollView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  NSLayoutYAxisAnchor* scrollViewTopAnchor;
  CGFloat scrollViewTopConstant = 0;
  if (self.hasNavigationBar) {
    scrollViewTopAnchor = self.navigationBar.bottomAnchor;
  } else {
    scrollViewTopAnchor = self.view.safeAreaLayoutGuide.topAnchor;
    scrollViewTopConstant = self.customSpacingBeforeImageIfNoNavigationBar;
  }
  if (self.topAlignedLayout) {
    [self.scrollView.topAnchor constraintEqualToAnchor:scrollViewTopAnchor
                                              constant:scrollViewTopConstant]
        .active = YES;
  } else {
    [self.scrollView.topAnchor
        constraintGreaterThanOrEqualToAnchor:scrollViewTopAnchor
                                    constant:scrollViewTopConstant]
        .active = YES;

    // Scroll View constraint to the vertical center.
    NSLayoutConstraint* centerYConstraint = [self.scrollView.centerYAnchor
        constraintEqualToAnchor:margins.centerYAnchor];
    // This needs to be lower than the height constraint, so it's deprioritized.
    // If this breaks, the scroll view is still constrained to the navigation
    // bar and the bottom safe area or button.
    centerYConstraint.priority = heightConstraint.priority - 1;
    centerYConstraint.active = YES;
  }

  if (!self.imageHasFixedSize || self.image) {
    // Constrain the image to the scroll view size and its aspect ratio.
    [self.imageView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [self.imageView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:UILayoutConstraintAxisVertical];
    CGFloat imageAspectRatio =
        self.imageView.image.size.width / self.imageView.image.size.height;

    self.imageViewAspectRatioConstraint = [self.imageView.widthAnchor
        constraintEqualToAnchor:self.imageView.heightAnchor
                     multiplier:imageAspectRatio];
    self.imageViewAspectRatioConstraint.active = YES;
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Flash the scroll indicators when the view appeared.
  [self.scrollView flashScrollIndicators];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  // Update fonts for specific content sizes.
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    self.primaryActionButton.titleLabel.font =
        PreferredFontForTextStyleWithMaxCategory(
            UIFontTextStyleHeadline,
            self.traitCollection.preferredContentSizeCategory,
            UIContentSizeCategoryExtraExtraExtraLarge);
  }

  // Update constraints for different size classes.
  BOOL hasNewHorizontalSizeClass =
      previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass;
  BOOL hasNewVerticalSizeClass = previousTraitCollection.verticalSizeClass !=
                                 self.traitCollection.verticalSizeClass;

  if (hasNewHorizontalSizeClass || hasNewVerticalSizeClass) {
    [self.view setNeedsUpdateConstraints];
  }
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self.view setNeedsUpdateConstraints];
}

- (void)viewLayoutMarginsDidChange {
  [super viewLayoutMarginsDidChange];
  [self.view setNeedsUpdateConstraints];
}

- (void)updateViewConstraints {
  BOOL isVerticalCompact =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;

  // Hiding the image causes the UIStackView to change the image's height to 0.
  // Because its width and height are related, if the aspect ratio constraint
  // is active, the image's width also goes to 0, which causes the stack view
  // width to become 0 too.
  [self.imageView setHidden:isVerticalCompact];
  [self.imageContainerView setHidden:isVerticalCompact];
  self.imageViewAspectRatioConstraint.active = !isVerticalCompact;

  // Allow the navigation bar to update its height based on new layout.
  [self.navigationBar invalidateIntrinsicContentSize];

  [super updateViewConstraints];
}

- (void)customizeSecondaryTitle:(UITextView*)secondaryTitle {
  // Do nothing by default. Subclasses can override this.
}

- (void)customizeSubtitle:(UITextView*)subtitle {
  // Do nothing by default. Subclasses can override this.
}

- (void)updateCustomGradientViewHeight:(CGFloat)height {
  self.customGradientViewHeight = height;
  self.gradientViewHeightConstraint.active = NO;
  self.gradientViewHeightConstraint = [self.gradientView.heightAnchor
      constraintEqualToConstant:self.customGradientViewHeight];
  self.gradientViewHeightConstraint.active = YES;
}

- (void)changeScrollViewBottomAnchorConstant:(CGFloat)constant {
  self.scrollViewBottomAnchorConstraint.constant = constant;
}

- (void)resetScrollViewBottomAnchorConstant {
  [self changeScrollViewBottomAnchorConstant:-kScrollViewBottomInsets];
}

#pragma mark - Private

// Handle taps on the dismiss button.
- (void)didTapDismissBarButton {
  DCHECK(self.showDismissBarButton);
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertDismissAction)]) {
    [self.actionHandler confirmationAlertDismissAction];
  }
}

// Handle taps on the help button.
- (void)didTapHelpButton {
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertLearnMoreAction)]) {
    [self.actionHandler confirmationAlertLearnMoreAction];
  }
}

// Handle taps on the primary action button.
- (void)didTapPrimaryActionButton {
  [self.actionHandler confirmationAlertPrimaryAction];
}

// Handle taps on the secondary action button
- (void)didTapSecondaryActionButton {
  DCHECK(self.secondaryActionString);
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertSecondaryAction)]) {
    [self.actionHandler confirmationAlertSecondaryAction];
  }
}

- (void)didTapTertiaryActionButton {
  DCHECK(self.tertiaryActionString);
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertTertiaryAction)]) {
    [self.actionHandler confirmationAlertTertiaryAction];
  }
}

// Helper to create the navigation bar.
- (UINavigationBar*)createNavigationBar {
  UINavigationBar* navigationBar = [[UINavigationBar alloc] init];
  navigationBar.translucent = NO;
  [navigationBar setShadowImage:[[UIImage alloc] init]];
  [navigationBar setBarTintColor:[UIColor colorNamed:kPrimaryBackgroundColor]];

  UINavigationItem* navigationItem = [[UINavigationItem alloc] init];
  if (self.helpButtonAvailable) {
    UIBarButtonItem* helpButton =
        [[UIBarButtonItem alloc] initWithImage:[UIImage imageNamed:@"help_icon"]
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(didTapHelpButton)];
    navigationItem.leftBarButtonItem = helpButton;

    if (self.helpButtonAccessibilityLabel) {
      helpButton.isAccessibilityElement = YES;
      helpButton.accessibilityLabel = self.helpButtonAccessibilityLabel;
    }

    helpButton.accessibilityIdentifier =
        kConfirmationAlertMoreInfoAccessibilityIdentifier;
    // Set the help button as the left button item so it can be used as a
    // popover anchor.
    _helpButton = helpButton;
  }

  if (self.titleView) {
    navigationItem.titleView = self.titleView;
  }

  if (self.showDismissBarButton) {
    UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:self.dismissBarButtonSystemItem
                             target:self
                             action:@selector(didTapDismissBarButton)];
    navigationItem.rightBarButtonItem = dismissButton;
  }

  navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [navigationBar setItems:@[ navigationItem ]];

  return navigationBar;
}

- (void)setImage:(UIImage*)image {
  _image = image;
  _imageView.image = image;
}

- (void)setScrollEnabled:(BOOL)scrollEnabled {
  _scrollEnabled = scrollEnabled;
  if (_scrollView) {
    _scrollView.scrollEnabled = _scrollEnabled;
  }
}

// Helper to create the image view.
- (UIImageView*)createImageView {
  UIImageView* imageView = [[UIImageView alloc] initWithImage:self.image];
  imageView.contentMode = UIViewContentModeScaleAspectFit;

  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  return imageView;
}

// Helper to create the image view enclosed in a frame with a shadow and a
// corner badge with a green checkmark. `self.imageView` is set in this method.
- (UIView*)createImageContainerViewWithShadowAndBadge {
  UIImageView* faviconBadgeView = [[UIImageView alloc] init];
  faviconBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
  UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolBadgeImagePointSize
                          weight:UIImageSymbolWeightMedium
                           scale:UIImageSymbolScaleMedium];
  faviconBadgeView.image = [UIImage systemImageNamed:kCheckmarkSymbol
                                   withConfiguration:configuration];
  faviconBadgeView.tintColor = [UIColor colorNamed:kGreenColor];

  UIImageView* faviconView = [[UIImageView alloc] initWithImage:self.image];
  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconView.contentMode = UIViewContentModeScaleAspectFit;

  UIView* frameView = [[UIView alloc] init];
  frameView.translatesAutoresizingMaskIntoConstraints = NO;
  frameView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  frameView.layer.cornerRadius = kFaviconCornerRadius;
  frameView.layer.shadowOffset =
      CGSizeMake(kFaviconShadowOffsetX, kFaviconShadowOffsetY);
  frameView.layer.shadowRadius = kFaviconShadowRadius;
  frameView.layer.shadowOpacity = kFaviconShadowOpacity;
  [frameView addSubview:faviconView];

  UIView* containerView = [[UIView alloc] init];
  [containerView addSubview:frameView];
  [containerView addSubview:faviconBadgeView];

  if (self.imageEnclosedWithShadowWithoutBadge) {
    [faviconBadgeView setHidden:YES];
  }

  CGFloat faviconSideLength = self.customFaviconSideLength > 0
                                  ? self.customFaviconSideLength
                                  : kFaviconSideLength;

  [NSLayoutConstraint activateConstraints:@[
    // Size constraints.
    [frameView.widthAnchor constraintEqualToConstant:kFaviconFrameSideLength],
    [frameView.heightAnchor constraintEqualToConstant:kFaviconFrameSideLength],
    [faviconView.widthAnchor constraintEqualToConstant:faviconSideLength],
    [faviconView.heightAnchor constraintEqualToConstant:faviconSideLength],
    [faviconBadgeView.widthAnchor
        constraintEqualToConstant:kFaviconBadgeSideLength],
    [faviconBadgeView.heightAnchor
        constraintEqualToConstant:kFaviconBadgeSideLength],

    // Badge is on the upper right corner of the frame.
    [frameView.topAnchor
        constraintEqualToAnchor:faviconBadgeView.centerYAnchor],
    [frameView.trailingAnchor
        constraintEqualToAnchor:faviconBadgeView.centerXAnchor],

    // Favicon is centered in the frame.
    [frameView.centerXAnchor constraintEqualToAnchor:faviconView.centerXAnchor],
    [frameView.centerYAnchor constraintEqualToAnchor:faviconView.centerYAnchor],

    // Frame and badge define the whole view returned by this method.
    [containerView.leadingAnchor
        constraintEqualToAnchor:frameView.leadingAnchor
                       constant:-kFaviconBadgeSideLength / 2],
    [containerView.bottomAnchor constraintEqualToAnchor:frameView.bottomAnchor],
    [containerView.topAnchor
        constraintEqualToAnchor:faviconBadgeView.topAnchor],
    [containerView.trailingAnchor
        constraintEqualToAnchor:faviconBadgeView.trailingAnchor],
  ]];

  self.imageView = faviconView;
  return containerView;
}

// Creates a UITextView with subtitle defaults.
- (UITextView*)createTextView {
  UITextView* view = [[UITextView alloc] init];
  view.textAlignment = NSTextAlignmentCenter;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  view.adjustsFontForContentSizeCategory = YES;
  view.editable = NO;
  view.selectable = NO;
  view.scrollEnabled = NO;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  return view;
}

// Helper to create the title label.
- (UILabel*)createTitleLabel {
  if (!self.titleTextStyle) {
    self.titleTextStyle = UIFontTextStyleTitle1;
  }
  UILabel* title = [[UILabel alloc] init];
  title.numberOfLines = 0;
  UIFontDescriptor* descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:self.titleTextStyle];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* fontMetrics =
      [UIFontMetrics metricsForTextStyle:self.titleTextStyle];
  title.font = [fontMetrics scaledFontForFont:font];
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.text = self.titleString;
  title.textAlignment = NSTextAlignmentCenter;
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.adjustsFontForContentSizeCategory = YES;
  title.accessibilityIdentifier =
      kConfirmationAlertTitleAccessibilityIdentifier;
  return title;
}

// Helper to create the title description view.
- (UITextView*)createSecondaryTitleView {
  UITextView* secondaryTitle = [self createTextView];
  secondaryTitle.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  secondaryTitle.text = self.secondaryTitleString;
  secondaryTitle.textColor = [UIColor colorNamed:kTextPrimaryColor];
  secondaryTitle.accessibilityIdentifier =
      kConfirmationAlertSecondaryTitleAccessibilityIdentifier;
  [self customizeSecondaryTitle:secondaryTitle];
  return secondaryTitle;
}

// Helper to create the subtitle view.
- (UITextView*)createSubtitleView {
  UITextView* subtitle = [self createTextView];
  subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  subtitle.text = self.subtitleString;
  subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitle.accessibilityIdentifier =
      kConfirmationAlertSubtitleAccessibilityIdentifier;
  [self customizeSubtitle:subtitle];
  return subtitle;
}

- (BOOL)hasNavigationBar {
  return self.helpButtonAvailable || self.showDismissBarButton ||
         self.titleView;
}

// Helper to create the scroll view.
- (UIScrollView*)createScrollView {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.alwaysBounceVertical = NO;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.scrollEnabled = self.scrollEnabled;
  [scrollView
      setShowsVerticalScrollIndicator:self.showsVerticalScrollIndicator];
  return scrollView;
}

// Helper to create the gradient view.
- (GradientView*)createGradientView {
  GradientView* gradientView = [[GradientView alloc]
      initWithTopColor:[[UIColor colorNamed:kPrimaryBackgroundColor]
                           colorWithAlphaComponent:0]
           bottomColor:[UIColor colorNamed:kPrimaryBackgroundColor]];
  gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  return gradientView;
}

// Helper to create the stack view.
- (UIStackView*)createStackViewWithArrangedSubviews:
    (NSArray<UIView*>*)subviews {
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:subviews];
  [stackView setCustomSpacing:self.customSpacingAfterImage
                    afterView:self.imageContainerView];

  if (self.imageHasFixedSize) {
    stackView.alignment = UIStackViewAlignmentCenter;
  } else {
    stackView.alignment = UIStackViewAlignmentFill;
  }

  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.spacing = self.customSpacing;
  return stackView;
}

- (UIView*)createActionStackView {
  UIStackView* actionStackView = [[UIStackView alloc] init];
  actionStackView.alignment = UIStackViewAlignmentFill;
  actionStackView.axis = UILayoutConstraintAxisVertical;
  actionStackView.translatesAutoresizingMaskIntoConstraints = NO;

  if (self.primaryActionString) {
    self.primaryActionButton = [self createPrimaryActionButton];
    [actionStackView addArrangedSubview:self.primaryActionButton];
  }

  if (self.secondaryActionString) {
    self.secondaryActionButton = [self createSecondaryActionButton];
    [actionStackView addArrangedSubview:self.secondaryActionButton];
  }
  // Tertiary button should show above the primary one.
  if (self.tertiaryActionString) {
    self.tertiaryActionButton = [self createTertiaryButton];
    [actionStackView insertArrangedSubview:self.tertiaryActionButton atIndex:0];
  }
  return actionStackView;
}

// Helper to create the primary action button.
- (UIButton*)createPrimaryActionButton {
  UIButton* primaryActionButton = PrimaryActionButton(YES);
  [primaryActionButton addTarget:self
                          action:@selector(didTapPrimaryActionButton)
                forControlEvents:UIControlEventTouchUpInside];
  [primaryActionButton setTitle:self.primaryActionString
                       forState:UIControlStateNormal];
  primaryActionButton.accessibilityIdentifier =
      kConfirmationAlertPrimaryActionAccessibilityIdentifier;
  primaryActionButton.titleLabel.adjustsFontSizeToFitWidth = YES;

  return primaryActionButton;
}

// Helper to create the primary action button.
- (UIButton*)createSecondaryActionButton {
  DCHECK(self.secondaryActionString);
  UIButton* secondaryActionButton =
      [UIButton buttonWithType:UIButtonTypeSystem];
  [secondaryActionButton addTarget:self
                            action:@selector(didTapSecondaryActionButton)
                  forControlEvents:UIControlEventTouchUpInside];
  [secondaryActionButton setTitle:self.secondaryActionString
                         forState:UIControlStateNormal];
  [secondaryActionButton setBackgroundColor:[UIColor clearColor]];
  UIColor* titleColor = [UIColor colorNamed:self.secondaryActionTextColor
                                                ? self.secondaryActionTextColor
                                                : kBlueColor];
  [secondaryActionButton setTitleColor:titleColor
                              forState:UIControlStateNormal];

  // TODO(crbug.com/1418068): Replace with UIButtonConfiguration when min
  // deployment target is iOS 15.
  UIEdgeInsets contentInsets =
      UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  SetContentEdgeInsets(secondaryActionButton, contentInsets);

  secondaryActionButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  secondaryActionButton.titleLabel.adjustsFontForContentSizeCategory = NO;
  secondaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;
  secondaryActionButton.accessibilityIdentifier =
      kConfirmationAlertSecondaryActionAccessibilityIdentifier;
  secondaryActionButton.titleLabel.adjustsFontSizeToFitWidth = YES;

  secondaryActionButton.pointerInteractionEnabled = YES;
  secondaryActionButton.pointerStyleProvider =
      CreateOpaqueButtonPointerStyleProvider();

  return secondaryActionButton;
}

- (UIButton*)createTertiaryButton {
  DCHECK(self.tertiaryActionString);
  UIButton* tertiaryActionButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [tertiaryActionButton addTarget:self
                           action:@selector(didTapTertiaryActionButton)
                 forControlEvents:UIControlEventTouchUpInside];
  [tertiaryActionButton setTitle:self.tertiaryActionString
                        forState:UIControlStateNormal];

  // TODO(crbug.com/1418068): Replace with UIButtonConfiguration when min
  // deployment target is iOS 15.
  UIEdgeInsets contentInsets =
      UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  SetContentEdgeInsets(tertiaryActionButton, contentInsets);

  [tertiaryActionButton setBackgroundColor:[UIColor clearColor]];
  UIColor* titleColor = [UIColor colorNamed:kBlueColor];
  [tertiaryActionButton setTitleColor:titleColor forState:UIControlStateNormal];
  tertiaryActionButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  tertiaryActionButton.titleLabel.adjustsFontForContentSizeCategory = NO;
  tertiaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;
  tertiaryActionButton.accessibilityIdentifier =
      kConfirmationAlertTertiaryActionAccessibilityIdentifier;

  tertiaryActionButton.pointerInteractionEnabled = YES;
  tertiaryActionButton.pointerStyleProvider =
      CreateOpaqueButtonPointerStyleProvider();

  return tertiaryActionButton;
}

- (CGFloat)detentForPreferredHeightInContext:
    (id<UISheetPresentationControllerDetentResolutionContext>)context
    API_AVAILABLE(ios(16)) {
  // Only activate this detent in portrait orientation on iPhone.
  UITraitCollection* traitCollection = context.containerTraitCollection;
  if (traitCollection.horizontalSizeClass != UIUserInterfaceSizeClassCompact ||
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassRegular) {
    return UISheetPresentationControllerDetentInactive;
  }

  // Obtain container view from presentation controller directly because
  // this view may not have been added to its container view yet.
  UIView* containerView = self.sheetPresentationController.containerView;

  // Measure compressed height without safe area inset (detent values are
  // generally expressed without safe area insets).
  CGFloat fittingWidth = containerView.bounds.size.width;
  CGSize fittingSize =
      CGSizeMake(fittingWidth, UILayoutFittingCompressedSize.height);
  CGFloat height = [self.view systemLayoutSizeFittingSize:fittingSize].height;
  height -= containerView.safeAreaInsets.bottom;

  // Replace bottom margin calculated based on view's own safe area with bottom
  // margin calculated based on the safe area of the container view it will
  // eventually live in. This is needed in case the detent value is requested
  // before the view has been added to its superview.
  height -= MAX(kActionsBottomMargin, self.view.safeAreaInsets.bottom);
  height += MAX(kActionsBottomMargin, containerView.safeAreaInsets.bottom);

  // Make sure detent is not larger than 75% of the maximum detent value but at
  // least as large as a standard medium detent.
  height = MIN(height, 0.75 * context.maximumDetentValue);
  CGFloat mediumDetentHeight = [UISheetPresentationControllerDetent.mediumDetent
      resolvedValueInContext:context];
  height = MAX(height, mediumDetentHeight);
  return height;
}

@end
