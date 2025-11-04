// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

#import "base/check.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

const CGFloat kContentBottomInset = 20;
const CGFloat kStackViewSpacing = 8;
const CGFloat kStackViewSpacingAfterIllustration = 27;

// The name of the checkmark symbol in filled circle.
NSString* const kCheckmarkSymbol = @"checkmark.circle.fill";

// The size of the checkmark symbol in the confirmation state on the primary
// button.
const CGFloat kSymbolConfirmationCheckmarkPointSize = 17;

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

@interface ConfirmationAlertViewController () <ButtonStackActionDelegate>

// References to the UI properties that need to be updated when the trait
// collection changes.
@property(nonatomic, strong) UILayoutGuide* widthLayoutGuide;
@property(nonatomic, strong) UIStackView* stackView;
@property(nonatomic, strong) UINavigationBar* navigationBar;
@property(nonatomic, strong) UIImageView* imageView;
@property(nonatomic, strong) UIView* imageContainerView;
@property(nonatomic, strong) NSLayoutConstraint* imageViewAspectRatioConstraint;
@end

@implementation ConfirmationAlertViewController

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration {
  self = [super initWithConfiguration:configuration];
  if (self) {
    self.actionDelegate = self;
    _customSpacingAfterImage = kStackViewSpacingAfterIllustration;
    _customContentBottomInset = kContentBottomInset;
    _customSpacing = kStackViewSpacing;
    _showDismissBarButton = YES;
    _dismissBarButtonSystemItem = UIBarButtonSystemItemDone;
    _shouldFillInformationStack = NO;
    _imageBackgroundColor = [UIColor colorNamed:kBackgroundColor];
    _mainBackgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  }
  return self;
}

- (instancetype)init {
  return [self initWithConfiguration:[[ButtonStackConfiguration alloc] init]];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = self.mainBackgroundColor;

  if (self.hasNavigationBar) {
    self.navigationBar = [self createNavigationBar];
    [self.view addSubview:self.navigationBar];
    AddSameConstraintsToSides(
        self.navigationBar, self.view.safeAreaLayoutGuide,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  }

  NSMutableArray* stackSubviews = [[NSMutableArray alloc] init];

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
    [stackSubviews addObject:self.imageContainerView];
  }

  if (self.aboveTitleView) {
    [stackSubviews addObject:self.aboveTitleView];
  }

  if (self.titleString.length) {
    self.titleLabel = [self createTitleLabel];
    [stackSubviews addObject:self.titleLabel];
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
    self.underTitleView.accessibilityIdentifier =
        kConfirmationAlertUnderTitleViewAccessibilityIdentifier;
    [stackSubviews addObject:self.underTitleView];
  }

  CHECK(stackSubviews);

  self.stackView = [self createStackViewWithArrangedSubviews:stackSubviews];
  [self.contentView addSubview:self.stackView];

  self.view.preservesSuperviewLayoutMargins = YES;

  // Constraint top/bottom of the stack view to the content view. This defines
  // the content area. No need to contraint horizontally as we don't want
  // horizontal scroll.
  [NSLayoutConstraint activateConstraints:@[
    [self.stackView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor
                       constant:-self.customContentBottomInset]
  ]];

  CGFloat stackViewTopConstant = 0;
  if (!self.hasNavigationBar) {
    stackViewTopConstant = self.customSpacingBeforeImageIfNoNavigationBar;
  }
  if (self.topAlignedLayout) {
    [self.stackView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                             constant:stackViewTopConstant]
        .active = YES;
  } else {
    [self.stackView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                    constant:stackViewTopConstant]
        .active = YES;

    // Stack View constraint to the vertical center.
    NSLayoutConstraint* centerYConstraint = [self.stackView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor];
    // This needs to be lower than the height constraint, so it's deprioritized.
    centerYConstraint.priority = UILayoutPriorityDefaultHigh - 1;
    centerYConstraint.active = YES;
  }

  // Only add the constraint for imageView with an image that has a variable
  // size.
  if (self.image && !self.imageHasFixedSize) {
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

  [self updatePromoStyleWidth];

  NSArray<UITrait>* traits = @[
    UITraitPreferredContentSizeCategory.class, UITraitHorizontalSizeClass.class,
    UITraitVerticalSizeClass.class
  ];
  auto* __weak weakSelf = self;
  id handler = ^(id<UITraitEnvironment> traitEnvironment,
                 UITraitCollection* previousCollection) {
    [weakSelf updateRegisteredTraits:previousCollection];
  };
  [self.view registerForTraitChanges:traits withHandler:handler];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (self.hasNavigationBar) {
    UIScrollView* scrollView = (UIScrollView*)self.contentView.superview;
    CGFloat navBarHeight = self.navigationBar.frame.size.height;
    if (scrollView.contentInset.top != navBarHeight) {
      scrollView.contentInset = UIEdgeInsetsMake(navBarHeight, 0, 0, 0);
      scrollView.scrollIndicatorInsets =
          UIEdgeInsetsMake(navBarHeight, 0, 0, 0);
    }
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
  BOOL showImageView =
      self.alwaysShowImage || (self.traitCollection.verticalSizeClass !=
                               UIUserInterfaceSizeClassCompact);

  // Hiding the image causes the UIStackView to change the image's height to 0.
  // Because its width and height are related, if the aspect ratio constraint
  // is active, the image's width also goes to 0, which causes the stack view
  // width to become 0 too.
  [self.imageView setHidden:!showImageView];
  [self.imageContainerView setHidden:!showImageView];
  self.imageViewAspectRatioConstraint.active = showImageView;

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

- (CGFloat)preferredHeightForContent {
  // Calculate the available width for the content from the layout guide.
  // This is more reliable than self.stackView.bounds.size.width before a layout
  // pass.
  CGFloat availableWidth = self.widthLayoutGuide.layoutFrame.size.width;

  // Calculate the height of the main content stack view constrained by the
  // available width.
  CGFloat height =
      [self.stackView
            systemLayoutSizeFittingSize:CGSizeMake(availableWidth,
                                                   UILayoutFittingCompressedSize
                                                       .height)
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;

  height += [super buttonStackHeight];

  if (self.navigationBar) {
    // Ask the navigation bar for its intrinsic height instead of relying on its
    // frame.
    height += [self.navigationBar
                  systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
                  .height;
  } else {
    height += self.customSpacingBeforeImageIfNoNavigationBar;
  }

  height += self.customContentBottomInset;
  // The view's safe area might not be available when this method is called.
  // Using the presenting view controller's safe area is a reliable fallback.
  height += self.presentingViewController.view.safeAreaInsets.bottom;
  return height;
}

#pragma mark - ButtonStackActionDelegate

- (void)didTapPrimaryActionButton {
  [self.actionHandler confirmationAlertPrimaryAction];
}

- (void)didTapSecondaryActionButton {
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertSecondaryAction)]) {
    [self.actionHandler confirmationAlertSecondaryAction];
  }
}

- (void)didTapTertiaryActionButton {
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertTertiaryAction)]) {
    [self.actionHandler confirmationAlertTertiaryAction];
  }
}

#pragma mark - Private

- (CGFloat)detentForPreferredHeightInContext:
    (id<UISheetPresentationControllerDetentResolutionContext>)context
    API_AVAILABLE(ios(16)) {
  // Only activate this detent in portrait orientation on iPhone.
  UITraitCollection* traitCollection = context.containerTraitCollection;
  if (traitCollection.horizontalSizeClass != UIUserInterfaceSizeClassCompact ||
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassRegular) {
    return UISheetPresentationControllerDetentInactive;
  }

  CGFloat height = [self preferredHeightForContent];

  // Make sure detent is not larger than 75% of the maximum detent value but at
  // least as large as a standard medium detent.
  height = MIN(height, 0.75 * context.maximumDetentValue);
  CGFloat mediumDetentHeight =
      [[UISheetPresentationControllerDetent mediumDetent]
          resolvedValueInContext:context];
  height = MAX(height, mediumDetentHeight);
  return height;
}

// Handle taps on the dismiss button.
- (void)didTapDismissBarButton {
  CHECK(self.showDismissBarButton);
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertDismissAction)]) {
    [self.actionHandler confirmationAlertDismissAction];
  }
}

// Helper to create the navigation bar.
- (UINavigationBar*)createNavigationBar {
  UINavigationBar* navigationBar = [[UINavigationBar alloc] init];
  navigationBar.translucent =
      CGColorGetAlpha(self.mainBackgroundColor.CGColor) < 1.0;
  [navigationBar setShadowImage:[[UIImage alloc] init]];
  [navigationBar setBarTintColor:self.mainBackgroundColor];

  UINavigationItem* navigationItem = [[UINavigationItem alloc] init];

  if (self.titleView) {
    navigationItem.titleView = self.titleView;
  }

  if (self.showDismissBarButton) {
    UIBarButtonItem* dismissButton;
    if (self.customDismissBarButtonImage) {
      dismissButton = [[UIBarButtonItem alloc]
          initWithImage:self.customDismissBarButtonImage
                  style:UIBarButtonItemStylePlain
                 target:self
                 action:@selector(didTapDismissBarButton)];
    } else {
      dismissButton = [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:self.dismissBarButtonSystemItem
                               target:self
                               action:@selector(didTapDismissBarButton)];
    }
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

// Helper to create the image view.
- (UIImageView*)createImageView {
  UIImageView* imageView = [[UIImageView alloc] initWithImage:self.image];
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  if (self.imageViewAccessibilityLabel) {
    imageView.isAccessibilityElement = YES;
    imageView.accessibilityLabel = self.imageViewAccessibilityLabel;
  }

  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  return imageView;
}

// Helper to create the image view enclosed in a frame with a shadow and a
// corner badge with a green checkmark. `self.imageView` is set in this method.
- (UIView*)createImageContainerViewWithShadowAndBadge {
  UIImageView* faviconBadgeView = [[UIImageView alloc] init];
  UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolConfirmationCheckmarkPointSize
                          weight:UIImageSymbolWeightMedium
                           scale:UIImageSymbolScaleMedium];
  // Use the system symbol name directly to avoid a dependency on the browser
  // layer's symbol helpers.
  faviconBadgeView.image = [UIImage systemImageNamed:kCheckmarkSymbol
                                   withConfiguration:configuration];
  faviconBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconBadgeView.tintColor = [UIColor colorNamed:kGreenColor];

  UIImageView* faviconView = [[UIImageView alloc] initWithImage:self.image];
  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconView.contentMode = UIViewContentModeScaleAspectFit;

  UIView* frameView = [[UIView alloc] init];
  frameView.translatesAutoresizingMaskIntoConstraints = NO;
  frameView.backgroundColor = _imageBackgroundColor;
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
  view.backgroundColor = self.mainBackgroundColor;
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
  title.accessibilityTraits = UIAccessibilityTraitHeader;
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
  if (!self.subtitleTextStyle) {
    self.subtitleTextStyle = UIFontTextStyleBody;
  }
  UITextView* subtitle = [self createTextView];
  subtitle.font = [UIFont preferredFontForTextStyle:self.subtitleTextStyle];
  subtitle.text = self.subtitleString;
  subtitle.textColor =
      self.subtitleTextColor ?: [UIColor colorNamed:kTextSecondaryColor];
  subtitle.accessibilityIdentifier =
      kConfirmationAlertSubtitleAccessibilityIdentifier;
  [self customizeSubtitle:subtitle];
  return subtitle;
}

- (BOOL)hasNavigationBar {
  return self.showDismissBarButton || self.titleView;
}

// Helper to create the stack view.
- (UIStackView*)createStackViewWithArrangedSubviews:
    (NSArray<UIView*>*)subviews {
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:subviews];
  [stackView setCustomSpacing:self.customSpacingAfterImage
                    afterView:self.imageContainerView];

  if (self.imageHasFixedSize && !self.shouldFillInformationStack) {
    stackView.alignment = UIStackViewAlignmentCenter;
  } else {
    stackView.alignment = UIStackViewAlignmentFill;
  }

  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.spacing = self.customSpacing;
  return stackView;
}



// Update the width of the content area and action buttons to match
// `PromoStyleViewController`. Should be invoked on `-viewDidLoad` to setup the
// initial width, and also when the horizontal size class changes.
- (void)updatePromoStyleWidth {
  if (self.widthLayoutGuide) {
    [self.view removeLayoutGuide:self.widthLayoutGuide];
  }
  self.widthLayoutGuide = AddPromoStyleWidthLayoutGuide(self.view);
  [NSLayoutConstraint activateConstraints:@[
    [self.stackView.leadingAnchor
        constraintEqualToAnchor:self.widthLayoutGuide.leadingAnchor],
    // Width Scroll View constraint for regular mode.
    [self.stackView.trailingAnchor
        constraintEqualToAnchor:self.widthLayoutGuide.trailingAnchor],
  ]];
}

// Checks which trait has been changed and adapts the UI to reflect this new
// environment.
- (void)updateRegisteredTraits:(UITraitCollection*)previousTraitCollection {
  // Update constraints for different size classes.
  BOOL hasNewHorizontalSizeClass =
      previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass;
  BOOL hasNewVerticalSizeClass = previousTraitCollection.verticalSizeClass !=
                                 self.traitCollection.verticalSizeClass;

  if (hasNewHorizontalSizeClass || hasNewVerticalSizeClass) {
    [self.view setNeedsUpdateConstraints];
    [self updatePromoStyleWidth];
  }
}

@end
