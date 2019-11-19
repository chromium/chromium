// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_view_controller.h"

#import "ios/chrome/browser/ui/passwords/password_breach_action_handler.h"
#include "ios/chrome/browser/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

namespace {
constexpr CGFloat kButtonVerticalInsets = 17;
constexpr CGFloat kPrimaryButtonCornerRadius = 13;
constexpr CGFloat kStackViewSpacing = 8;
constexpr CGFloat kStackViewSpacingAfterIllustration = 27;
// The multiplier used when in regular horizontal size class.
constexpr CGFloat kSafeAreaMultiplier = 0.8;
}  // namespace

@interface PasswordBreachViewController () <UIToolbarDelegate>

// Properties backing up the respective consumer setter.
@property(nonatomic, strong) NSString* titleString;
@property(nonatomic, strong) NSString* subtitleString;
@property(nonatomic, strong) NSString* primaryActionString;
@property(nonatomic) BOOL primaryActionAvailable;

// References to the UI properties that need to be updated when the trait
// collection changes.
@property(nonatomic, strong) UIButton* primaryActionButton;
@property(nonatomic, strong) UIImageView* imageView;
// Constraints.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* compactWidthConstraints;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* regularWidthConstraints;
@property(nonatomic, strong)
    NSLayoutConstraint* scrollViewBottomVerticalConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* primaryButtonBottomVerticalConstraint;
@end

@implementation PasswordBreachViewController

#pragma mark - Public

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  UIToolbar* topToolbar = [self createTopToolbar];
  [self.view addSubview:topToolbar];

  self.imageView = [self createImageView];
  UILabel* title = [self createTitleLabel];
  UILabel* subtitle = [self createSubtitleLabel];

  UIStackView* stackView = [self
      createStackViewWithArrangedSubviews:@[ self.imageView, title, subtitle ]];

  UIScrollView* scrollView = [self createScrollView];
  [scrollView addSubview:stackView];
  [self.view addSubview:scrollView];

  self.view.preservesSuperviewLayoutMargins = YES;
  UILayoutGuide* margins = self.view.layoutMarginsGuide;

  // Toolbar constraints to the top.
  AddSameConstraintsToSides(
      topToolbar, self.view.safeAreaLayoutGuide,
      LayoutSides::kTrailing | LayoutSides::kTop | LayoutSides::kLeading);

  // Scroll View constraints to the height of its content. Can be overridden.
  NSLayoutConstraint* heightConstraint = [scrollView.heightAnchor
      constraintEqualToAnchor:scrollView.contentLayoutGuide.heightAnchor];
  // UILayoutPriorityDefaultHigh is the default priority for content
  // compression. Setting this lower avoids compressing the content of the
  // scroll view.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;

  // Scroll View constraint to the vertical center. Can be overridden.
  NSLayoutConstraint* centerYConstraint =
      [scrollView.centerYAnchor constraintEqualToAnchor:margins.centerYAnchor];
  // This needs to be lower than the height constraint, so it's deprioritized.
  // If this breaks, the scroll view is still constrained to the top toolbar and
  // the bottom safe area or button.
  centerYConstraint.priority = heightConstraint.priority - 1;
  centerYConstraint.active = YES;

  // Constraint the content of the scroll view to the size of the stack view.
  // This defines the content area.
  AddSameConstraints(stackView, scrollView);

  // Disable horizontal scrolling and constraint the content size to the scroll
  // view size.
  [scrollView.widthAnchor
      constraintEqualToAnchor:scrollView.contentLayoutGuide.widthAnchor]
      .active = YES;

  [scrollView.centerXAnchor constraintEqualToAnchor:margins.centerXAnchor]
      .active = YES;

  // Width Scroll View constraint. It changes based on the size class.
  self.compactWidthConstraints = @[
    [scrollView.widthAnchor constraintEqualToAnchor:margins.widthAnchor],
  ];
  self.regularWidthConstraints = @[
    [scrollView.widthAnchor constraintEqualToAnchor:margins.widthAnchor
                                         multiplier:kSafeAreaMultiplier],
  ];

  // The bottom anchor for the scroll view. It will be updated to the button top
  // anchor if it exists.
  NSLayoutYAxisAnchor* scrollViewBottomAnchor =
      self.view.safeAreaLayoutGuide.bottomAnchor;

  if (self.primaryActionAvailable) {
    UIButton* primaryActionButton = [self createPrimaryActionButton];
    [self.view addSubview:primaryActionButton];

    // Primary Action Button constraints.
    self.primaryButtonBottomVerticalConstraint =
        [primaryActionButton.bottomAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
    [NSLayoutConstraint activateConstraints:@[
      [primaryActionButton.leadingAnchor
          constraintEqualToAnchor:scrollView.leadingAnchor],
      [primaryActionButton.trailingAnchor
          constraintEqualToAnchor:scrollView.trailingAnchor],
      self.primaryButtonBottomVerticalConstraint,
    ]];

    scrollViewBottomAnchor = primaryActionButton.topAnchor;
    self.primaryActionButton = primaryActionButton;
  }

  // Constraing the image to the scroll view size and its aspect ratio.
  [self.imageView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [self.imageView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  CGFloat imageAspectRatio =
      self.imageView.image.size.width / self.imageView.image.size.height;
  self.scrollViewBottomVerticalConstraint = [scrollView.bottomAnchor
      constraintLessThanOrEqualToAnchor:scrollViewBottomAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [self.imageView.widthAnchor
        constraintEqualToAnchor:self.imageView.heightAnchor
                     multiplier:imageAspectRatio],
    [scrollView.topAnchor
        constraintGreaterThanOrEqualToAnchor:topToolbar.bottomAnchor],
    self.scrollViewBottomVerticalConstraint,
  ]];
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
  CGFloat marginValue =
      self.view.layoutMargins.left - self.view.safeAreaInsets.left;
  self.scrollViewBottomVerticalConstraint.constant = -marginValue;
  self.primaryButtonBottomVerticalConstraint.constant = -marginValue;
  if (self.traitCollection.horizontalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    [NSLayoutConstraint deactivateConstraints:self.regularWidthConstraints];
    [NSLayoutConstraint activateConstraints:self.compactWidthConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:self.compactWidthConstraints];
    [NSLayoutConstraint activateConstraints:self.regularWidthConstraints];
  }
  self.imageView.hidden =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
  [super updateViewConstraints];
}

#pragma mark - PasswordBreachConsumer

- (void)setTitleString:(NSString*)titleString
            subtitleString:(NSString*)subtitleString
       primaryActionString:(NSString*)primaryActionString
    primaryActionAvailable:(BOOL)primaryActionAvailable {
  self.titleString = titleString;
  self.subtitleString = subtitleString;
  self.primaryActionString = primaryActionString;
  self.primaryActionAvailable = primaryActionAvailable;
}

#pragma mark - UIToolbarDelegate

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  return UIBarPositionTopAttached;
}

#pragma mark - Private

// Handle taps on the done button.
- (void)didTapDoneButton {
  [self.actionHandler passwordBreachDone];
}

// Handle taps on the primary action button.
- (void)didTapPrimaryActionButton {
  [self.actionHandler passwordBreachPrimaryAction];
}

// Helper to create the top toolbar.
- (UIToolbar*)createTopToolbar {
  UIToolbar* topToolbar = [[UIToolbar alloc] init];
  topToolbar.translucent = NO;
  [topToolbar setShadowImage:[[UIImage alloc] init]
          forToolbarPosition:UIBarPositionAny];
  [topToolbar setBarTintColor:[UIColor colorNamed:kBackgroundColor]];
  topToolbar.delegate = self;
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(didTapDoneButton)];
  UIBarButtonItem* spacer = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  topToolbar.items = @[ spacer, doneButton ];
  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  return topToolbar;
}

// Helper to create the image view.
- (UIImageView*)createImageView {
  UIImage* image = [UIImage imageNamed:@"password_breach_illustration"];
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  return imageView;
}

// Helper to create the title label.
- (UILabel*)createTitleLabel {
  UILabel* title = [[UILabel alloc] init];
  title.numberOfLines = 0;
  UIFontDescriptor* descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleTitle1];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* fontMetrics =
      [UIFontMetrics metricsForTextStyle:UIFontTextStyleTitle1];
  title.font = [fontMetrics scaledFontForFont:font];
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.text = self.titleString.capitalizedString;
  title.textAlignment = NSTextAlignmentCenter;
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.adjustsFontForContentSizeCategory = YES;
  return title;
}

// Helper to create the subtitle label.
- (UILabel*)createSubtitleLabel {
  UILabel* subtitle = [[UILabel alloc] init];
  subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  subtitle.numberOfLines = 0;
  subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitle.text = self.subtitleString;
  subtitle.textAlignment = NSTextAlignmentCenter;
  subtitle.translatesAutoresizingMaskIntoConstraints = NO;
  subtitle.adjustsFontForContentSizeCategory = YES;
  return subtitle;
}

// Helper to create the scroll view.
- (UIScrollView*)createScrollView {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.alwaysBounceVertical = NO;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  return scrollView;
}

// Helper to create the stack view.
- (UIStackView*)createStackViewWithArrangedSubviews:
    (NSArray<UIView*>*)subviews {
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:subviews];
  [stackView setCustomSpacing:kStackViewSpacingAfterIllustration
                    afterView:self.imageView];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.spacing = kStackViewSpacing;
  return stackView;
}

// Helper to create the primary action button.
- (UIButton*)createPrimaryActionButton {
  UIButton* primaryActionButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [primaryActionButton addTarget:self
                          action:@selector(didTapPrimaryActionButton)
                forControlEvents:UIControlEventTouchUpInside];
  [primaryActionButton setTitle:self.primaryActionString.capitalizedString
                       forState:UIControlStateNormal];
  primaryActionButton.contentEdgeInsets =
      UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  [primaryActionButton setBackgroundColor:[UIColor colorNamed:kBlueColor]];
  UIColor* titleColor = [UIColor colorNamed:kSolidButtonTextColor];
  [primaryActionButton setTitleColor:titleColor forState:UIControlStateNormal];
  primaryActionButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  primaryActionButton.layer.cornerRadius = kPrimaryButtonCornerRadius;
  primaryActionButton.titleLabel.adjustsFontForContentSizeCategory = NO;
  primaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;
  return primaryActionButton;
}

@end
