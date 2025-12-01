// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

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
@property(nonatomic, strong) UIStackView* stackView;
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
    _customSpacing = kStackViewSpacing;
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

  // Constraint the stack view to the content view.
  [NSLayoutConstraint activateConstraints:@[
    [self.stackView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
    [self.stackView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor],
    [self.stackView.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor],
  ]];
  CGFloat stackViewTopConstant = self.customSpacingBeforeImageIfNoNavigationBar;
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

  [super updateViewConstraints];
}

- (void)customizeSecondaryTitle:(UITextView*)secondaryTitle {
  // Do nothing by default. Subclasses can override this.
}

- (void)customizeSubtitle:(UITextView*)subtitle {
  // Do nothing by default. Subclasses can override this.
}

- (CGFloat)preferredHeightForContent {
  CGFloat height = [super preferredHeightForContent];

    height += self.customSpacingBeforeImageIfNoNavigationBar;

  return height;
}

#pragma mark - ButtonStackActionDelegate

- (void)didTapPrimaryActionButton {
  [self.actionHandler confirmationAlertPrimaryAction];
  base::UmaHistogramEnumeration(
      "IOS.ConfirmationAlertSheet.Outcome",
      ConfirmationAlertSheetAction::kPrimaryButtonTapped);
}

- (void)didTapSecondaryActionButton {
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertSecondaryAction)]) {
    [self.actionHandler confirmationAlertSecondaryAction];
    base::UmaHistogramEnumeration(
        "IOS.ConfirmationAlertSheet.Outcome",
        ConfirmationAlertSheetAction::kSecondaryButtonTapped);
  }
}

- (void)didTapTertiaryActionButton {
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertTertiaryAction)]) {
    [self.actionHandler confirmationAlertTertiaryAction];
    base::UmaHistogramEnumeration(
        "IOS.ConfirmationAlertSheet.Outcome",
        ConfirmationAlertSheetAction::kTertiaryButtonTapped);
  }
}

#pragma mark - Private

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
  }
}

@end
