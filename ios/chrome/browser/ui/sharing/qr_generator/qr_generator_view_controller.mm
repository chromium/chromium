// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/qr_generator/qr_generator_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/sharing/qr_generator/qr_generator_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Height and width of the QR code image, in points.
const CGFloat kQRCodeImageSize = 200.0;
constexpr CGFloat kGeneratedImagePadding = 20;
constexpr CGFloat kButtonMaxWidth = 327;
constexpr CGFloat kContentMaxWidth = 500;
constexpr CGFloat kBottomMargin = 24;
constexpr CGFloat kSymbolSize = 22;

}  // namespace

@interface QRGeneratorViewController ()

// Container view that will wrap the views making up the content.
@property(nonatomic, strong) UIStackView* stackView;

// URL of the page to generate a QR code for.
@property(nonatomic, copy) NSURL* pageURL;

@property(nonatomic, copy) NSString* pageTitle;

@property(nonatomic, strong) NSArray* regularHeightToolbarItems;
@property(nonatomic, strong) NSArray* compactHeightToolbarItems;
@property(nonatomic, strong) UIToolbar* topToolbar;

@property(nonatomic, strong)
    NSLayoutConstraint* regularHeightScrollViewBottomVerticalConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* compactHeightScrollViewBottomVerticalConstraint;

@end

@implementation QRGeneratorViewController

- (instancetype)initWithTitle:(NSString*)title pageURL:(NSURL*)pageURL {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _pageURL = pageURL;
    _pageTitle = title;
  }
  return self;
}

#pragma mark - Properties

- (UIImage*)content {
  UIEdgeInsets padding =
      UIEdgeInsetsMake(kGeneratedImagePadding, kGeneratedImagePadding,
                       kGeneratedImagePadding, kGeneratedImagePadding);
  return ImageFromView(self.stackView, self.view.backgroundColor, padding);
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  UIToolbar* topToolbar = [self createTopToolbar];
  self.topToolbar = topToolbar;
  [self.view addSubview:topToolbar];

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];

  UIView* imageView = [self createImageView];
  UILabel* title = [self createTitleLabel];
  UILabel* subtitle = [self createSubtitleLabel];
  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ imageView, title, subtitle ]];
  self.stackView = stackView;
  stackView.spacing = 8;
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.alignment = UIStackViewAlignmentCenter;
  [scrollView addSubview:stackView];

  UIView* primaryActionButton = [self createPrimaryActionButton];
  _primaryActionButton = primaryActionButton;
  [self.view addSubview:primaryActionButton];

  // Toolbar constraints to the top.
  AddSameConstraintsToSides(
      topToolbar, self.view.safeAreaLayoutGuide,
      LayoutSides::kTrailing | LayoutSides::kTop | LayoutSides::kLeading);

  // Content size of the scrollview.
  AddSameConstraintsWithInsets(stackView, scrollView,
                               NSDirectionalEdgeInsetsMake(0, 0, 20, 0));
  // Scroll View constraints to the height of its content. Can be overridden.
  NSLayoutConstraint* heightConstraint = [scrollView.heightAnchor
      constraintEqualToAnchor:scrollView.contentLayoutGuide.heightAnchor];
  // UILayoutPriorityDefaultHigh is the default priority for content
  // compression. Setting this lower avoids compressing the content of the
  // scroll view.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;

  NSLayoutConstraint* stackViewWidth = [stackView.widthAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.widthAnchor];
  stackViewWidth.priority = UILayoutPriorityRequired - 1;

  NSLayoutConstraint* lowPriorityWidthConstraint =
      [primaryActionButton.widthAnchor
          constraintEqualToConstant:kButtonMaxWidth];
  lowPriorityWidthConstraint.priority = UILayoutPriorityDefaultHigh;

  NSLayoutConstraint* scrollViewYCenter = [scrollView.centerYAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerYAnchor];
  scrollViewYCenter.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [scrollView.topAnchor
        constraintGreaterThanOrEqualToAnchor:topToolbar.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    scrollViewYCenter,

    [stackView.widthAnchor
        constraintLessThanOrEqualToConstant:kContentMaxWidth],
    [stackView.centerXAnchor constraintEqualToAnchor:scrollView.centerXAnchor],

    [primaryActionButton.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-kBottomMargin],
    [primaryActionButton.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:scrollView.leadingAnchor],
    [primaryActionButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:scrollView.trailingAnchor],
    [primaryActionButton.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    lowPriorityWidthConstraint

  ]];

  self.regularHeightScrollViewBottomVerticalConstraint =
      [scrollView.bottomAnchor
          constraintLessThanOrEqualToAnchor:primaryActionButton.topAnchor
                                   constant:-8];
  self.compactHeightScrollViewBottomVerticalConstraint =
      [scrollView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                .bottomAnchor
                                   constant:-8];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitVerticalSizeClass.self ]);
    [self registerForTraitChanges:traits
                       withTarget:self.view
                           action:@selector(setNeedsUpdateConstraints)];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  // Update constraints for different size classes.
  BOOL hasNewVerticalSizeClass = previousTraitCollection.verticalSizeClass !=
                                 self.traitCollection.verticalSizeClass;

  if (hasNewVerticalSizeClass) {
    [self.view setNeedsUpdateConstraints];
  }
}
#endif

- (void)updateViewConstraints {
  BOOL isVerticalCompact =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;

  [self.primaryActionButton setHidden:isVerticalCompact];

  NSLayoutConstraint* oldBottomConstraint;
  NSLayoutConstraint* newBottomConstraint;
  if (isVerticalCompact) {
    oldBottomConstraint = self.regularHeightScrollViewBottomVerticalConstraint;
    newBottomConstraint = self.compactHeightScrollViewBottomVerticalConstraint;

    // Use setItems:animated method instead of setting the items property, as
    // that causes issues with the Done button. See crbug.com/1082723
    [self.topToolbar setItems:self.compactHeightToolbarItems animated:YES];
  } else {
    oldBottomConstraint = self.compactHeightScrollViewBottomVerticalConstraint;
    newBottomConstraint = self.regularHeightScrollViewBottomVerticalConstraint;

    // Use setItems:animated method instead of setting the items property, as
    // that causes issues with the Done button. See crbug.com/1082723
    [self.topToolbar setItems:self.regularHeightToolbarItems animated:YES];
  }

  [NSLayoutConstraint deactivateConstraints:@[ oldBottomConstraint ]];
  [NSLayoutConstraint activateConstraints:@[ newBottomConstraint ]];

  // Allow toolbar to update its height based on new layout.
  [self.topToolbar invalidateIntrinsicContentSize];

  [super updateViewConstraints];
}

#pragma mark - Private Methods

- (UIImage*)createQRCodeImage {
  NSData* urlData =
      [[self.pageURL absoluteString] dataUsingEncoding:NSUTF8StringEncoding];
  return GenerateQRCode(urlData, kQRCodeImageSize);
}

// Helper to create the top toolbar.
- (UIToolbar*)createTopToolbar {
  UIToolbar* topToolbar = [[UIToolbar alloc] init];
  topToolbar.translucent = NO;
  [topToolbar setShadowImage:[[UIImage alloc] init]
          forToolbarPosition:UIBarPositionAny];
  [topToolbar setBarTintColor:[UIColor colorNamed:kBackgroundColor]];

  NSMutableArray* regularHeightItems = [[NSMutableArray alloc] init];
  NSMutableArray* compactHeightItems = [[NSMutableArray alloc] init];
  UIImage* helpImage = DefaultSymbolWithPointSize(kHelpSymbol, kSymbolSize);
  UIBarButtonItem* helpButton =
      [[UIBarButtonItem alloc] initWithImage:helpImage
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(didTapHelpButton)];
  [regularHeightItems addObject:helpButton];
  [compactHeightItems addObject:helpButton];

  helpButton.isAccessibilityElement = YES;
  helpButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HELP_ACCESSIBILITY_LABEL);

  // Set the help button as the left button item so it can be used as a
  // popover anchor.
  _helpButton = helpButton;

  // Add margin with help button.
  UIBarButtonItem* fixedSpacer = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace
                           target:nil
                           action:nil];
  fixedSpacer.width = 15.0f;
  [compactHeightItems addObject:fixedSpacer];

  UIBarButtonItem* primaryActionBarButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemAction
                           target:self
                           action:@selector(didTapPrimaryActionButton)];

  // Only shows up in constraint height mode.
  [compactHeightItems addObject:primaryActionBarButton];

  UIBarButtonItem* spacer = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  [regularHeightItems addObject:spacer];
  [compactHeightItems addObject:spacer];

  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(didTapDismissBarButton)];
  [regularHeightItems addObject:dismissButton];
  [compactHeightItems addObject:dismissButton];

  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  self.regularHeightToolbarItems = regularHeightItems;
  self.compactHeightToolbarItems = compactHeightItems;

  return topToolbar;
}

// Handles taps on the dismiss button.
- (void)didTapDismissBarButton {
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertDismissAction)]) {
    [self.actionHandler confirmationAlertDismissAction];
  }
}

// Handles taps on the help button.
- (void)didTapHelpButton {
  if ([self.actionHandler
          respondsToSelector:@selector(confirmationAlertLearnMoreAction)]) {
    [self.actionHandler confirmationAlertLearnMoreAction];
  }
}

// Handles taps on the primary action button.
- (void)didTapPrimaryActionButton {
  [self.actionHandler confirmationAlertPrimaryAction];
}

// Helper to create the image view.
- (UIImageView*)createImageView {
  UIImageView* imageView =
      [[UIImageView alloc] initWithImage:[self createQRCodeImage]];
  imageView.contentMode = UIViewContentModeScaleAspectFit;

  imageView.isAccessibilityElement = YES;
  imageView.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_QR_CODE_ACCESSIBILITY_LABEL);

  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  return imageView;
}

// Helper to create the title label.
- (UILabel*)createTitleLabel {
  UILabel* title = [[UILabel alloc] init];
  title.numberOfLines = 0;
  UIFontDescriptor* descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleTitle3];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* fontMetrics =
      [UIFontMetrics metricsForTextStyle:UIFontTextStyleTitle3];
  title.font = [fontMetrics scaledFontForFont:font];
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.text = self.pageTitle;
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
  subtitle.text = [self.pageURL host];
  subtitle.textAlignment = NSTextAlignmentCenter;
  subtitle.translatesAutoresizingMaskIntoConstraints = NO;
  subtitle.adjustsFontForContentSizeCategory = YES;
  return subtitle;
}

// Helper to create the primary action button.
- (UIButton*)createPrimaryActionButton {
  UIButton* primaryActionButton = PrimaryActionButton(YES);
  [primaryActionButton addTarget:self
                          action:@selector(didTapPrimaryActionButton)
                forControlEvents:UIControlEventTouchUpInside];
  SetConfigurationTitle(primaryActionButton,
                        l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL));
  [primaryActionButton
      setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                        forAxis:UILayoutConstraintAxisVertical];

  return primaryActionButton;
}

@end
