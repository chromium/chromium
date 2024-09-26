// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_view_controller.h"

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/instruction_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/constants.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/password_auto_fill/password_auto_fill_api.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
CGFloat const kCaptionTextViewOffset = 16;
CGFloat const kDefaultMargin = 16;
CGFloat const kTitleTopMinimumMargin = 48;
CGFloat const kTitleHorizontalMargin = 18;
CGFloat const kDefaultBannerMultiplier = 0.25;
CGFloat const kContentWidthMultiplier = 0.65;
CGFloat const kBottomMargin = 10;
CGFloat const kButtonHorizontalMargin = 4;
CGFloat const kContentOptimalWidth = 327;
}  // namespace

@interface PasswordsInOtherAppsViewController ()

// Properties set on initialization.
@property(nonatomic, copy, readonly) NSString* titleText;
@property(nonatomic, copy, readonly) NSString* subtitleText;
// Whether banner is light or dark mode
@property(nonatomic, assign) UIUserInterfaceStyle bannerStyle;
@property(nonatomic, copy, readonly) NSString* bannerName;
@property(nonatomic, copy, readonly) NSString* actionString;

// Visible UI components.
@property(nonatomic, strong) UIImageView* imageView;
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UILabel* subtitleLabel;
@property(nonatomic, strong) UIView* turnOnInstructionView;
@property(nonatomic, strong) UIView* turnOffInstructionView;
@property(nonatomic, strong) HighlightButton* actionButton;

@property(nonatomic, strong) UIActivityIndicatorView* spinner;
// Views that are used to format the layout of visible UI components.
@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) UIView* scrollContentView;
@property(nonatomic, strong) UIView* specificContentView;

// Helper properties.
@property(nonatomic, assign, readonly) BOOL useShortInstruction;
@property(nonatomic, strong, readonly) NSArray<NSString*>* steps;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* turnOnInstructionViewConstraints;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* turnOffInstructionViewConstraints;
@property(nonatomic, strong) UINavigationBar* navigationBar;
@property(nonatomic, strong) UINavigationBarAppearance* defaultAppearance;

// Whether the image is currently being calculated; used to prevent infinite
// recursions caused by `viewDidLayoutSubviews`.
@property(nonatomic, assign) BOOL calculatingImageSize;
@end

@interface PasswordsInOtherAppsViewController () <
    UIAdaptivePresentationControllerDelegate,
    UITextViewDelegate>

@end

@implementation PasswordsInOtherAppsViewController

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _titleText =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS);
    _actionString = l10n_util::GetNSString(IDS_IOS_OPEN_SETTINGS);

    UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
    if (idiom == UIUserInterfaceIdiomPad) {
      _subtitleText = l10n_util::GetNSString(
          IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_SUBTITLE_IPAD);
    } else {
      _subtitleText = l10n_util::GetNSString(
          IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_SUBTITLE_IPHONE);
    }

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    _bannerName = kGoogleSettingsPasswordsInOtherAppsBannerImage;
#else
    _bannerName = kChromiumSettingsPasswordsInOtherAppsBannerImage;
#endif

    self.bannerStyle = UIUserInterfaceStyleUnspecified;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.accessibilityIdentifier =
      kPasswordsInOtherAppsViewAccessibilityIdentifier;
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  // Create a layout guide for the margin between the subtitle and the screen-
  // specific content. A layout guide is needed because the margin scales with
  // the view height.
  UILayoutGuide* subtitleMarginLayoutGuide = [[UILayoutGuide alloc] init];

  self.scrollContentView = [[UIView alloc] init];
  self.scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.scrollContentView addSubview:self.imageView];

  // Add the labels.
  [self.scrollContentView addSubview:self.titleLabel];
  [self.scrollContentView addSubview:self.subtitleLabel];
  [self.view addLayoutGuide:subtitleMarginLayoutGuide];
  [self.scrollContentView addSubview:self.specificContentView];

  // Wrap everything except the action buttons in a scroll view, to support
  // dynamic types.
  [self.scrollView addSubview:self.scrollContentView];
  [self.view addSubview:self.scrollView];

  [self updateInstructionsWithCurrentPasswordAutoFillStatus];

  // Create a layout guide to constrain the width of the content, while still
  // allowing the scroll view to take the full screen width.
  UILayoutGuide* widthLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:widthLayoutGuide];

  [NSLayoutConstraint activateConstraints:@[
    // Content width layout guide constraints. Constrain the width to both at
    // least 65% of the view width, and to the full view width with margins.
    // This is to accomodate the iPad layout, which cannot be isolated out using
    // the traitCollection because of the FormSheet presentation style
    // (iPad FormSheet is considered compact).
    [widthLayoutGuide.centerXAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerXAnchor],
    [widthLayoutGuide.widthAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                 .widthAnchor
                                  multiplier:kContentWidthMultiplier],
    [widthLayoutGuide.widthAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .widthAnchor
                                 constant:-2 * kDefaultMargin],

    // Scroll view constraints.
    [self.scrollView.topAnchor
        constraintEqualToAnchor:self.view
                                    .topAnchor],  // banner image should overlap
                                                  // with navigation bar.
    [self.scrollView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.scrollView.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],

    // Scroll content view constraints. Constrain its height to at least the
    // scroll view height, so that derived VCs can pin UI elements just above
    // the buttons.
    [self.scrollContentView.topAnchor
        constraintEqualToAnchor:self.scrollView.topAnchor],
    [self.scrollContentView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor],
    [self.scrollContentView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor],
    [self.scrollContentView.bottomAnchor
        constraintEqualToAnchor:self.scrollView.bottomAnchor],
    [self.scrollContentView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.scrollView.heightAnchor],

    // Banner image constraints. Scale the image vertically so its height takes
    // a certain % of the view height while maintaining its aspect ratio. Don't
    // constrain the width so that the image extends all the way to the edges of
    // the view, outside the scrollContentView.
    [self.imageView.topAnchor
        constraintEqualToAnchor:self.scrollContentView.topAnchor],
    [self.imageView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],

    // Labels contraints. Attach them to the top of the scroll content view, and
    // center them horizontally.
    [self.titleLabel.topAnchor
        constraintEqualToAnchor:self.imageView.bottomAnchor],
    [self.titleLabel.centerXAnchor
        constraintEqualToAnchor:self.scrollContentView.centerXAnchor],
    [self.titleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.scrollContentView.widthAnchor
                                 constant:-2 * kTitleHorizontalMargin],
    [self.subtitleLabel.topAnchor
        constraintEqualToAnchor:self.titleLabel.bottomAnchor
                       constant:kDefaultMargin],
    [self.subtitleLabel.centerXAnchor
        constraintEqualToAnchor:self.scrollContentView.centerXAnchor],
    [self.subtitleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.scrollContentView.widthAnchor],

    // Constraints for the screen-specific content view. It should take the
    // remaining scroll view area, with some margins on the top and sides.
    [subtitleMarginLayoutGuide.topAnchor
        constraintEqualToAnchor:self.subtitleLabel.bottomAnchor],
    [subtitleMarginLayoutGuide.heightAnchor
        constraintEqualToConstant:kDefaultMargin],
    [self.specificContentView.topAnchor
        constraintEqualToAnchor:subtitleMarginLayoutGuide.bottomAnchor],
    [self.specificContentView.leadingAnchor
        constraintEqualToAnchor:self.scrollContentView.leadingAnchor],
    [self.specificContentView.trailingAnchor
        constraintEqualToAnchor:self.scrollContentView.trailingAnchor],
    [self.specificContentView.bottomAnchor
        constraintEqualToAnchor:self.scrollContentView.bottomAnchor],
  ]];

  // This constraint is added to enforce that the content width should be as
  // close to the optimal width as possible, within the range already activated
  // for "widthLayoutGuide.widthAnchor" previously, with a higher priority.
  // In this case, the content width in iPad and iPhone landscape mode should be
  // the safe layout width multiplied by kContentWidthMultiplier, while the
  // content width for a iPhone portrait mode should be kContentOptimalWidth.
  NSLayoutConstraint* contentLayoutGuideWidthConstraint =
      [widthLayoutGuide.widthAnchor
          constraintEqualToConstant:kContentOptimalWidth];
  contentLayoutGuideWidthConstraint.priority = UILayoutPriorityRequired - 1;
  contentLayoutGuideWidthConstraint.active = YES;

  // In iPhone landscape mode, the top image is removed. In that case, we should
  // make sure there is enough distance between the title label and the top edge
  // of the iPhone.
  // We set to priority of this constraint to be lower than the imageView's
  // compression resistance priority so it could expand if the image height is
  // bigger than this.
  NSLayoutConstraint* imageHeightConstraint = [self.imageView.heightAnchor
      constraintEqualToConstant:kTitleTopMinimumMargin];
  imageHeightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  imageHeightConstraint.active = YES;
}

- (void)viewDidDisappear:(BOOL)animated {
  if (self.navigationBar) {
    self.navigationItem.rightBarButtonItem = nil;
    [self.navigationBar setBackgroundImage:nil
                             forBarMetrics:UIBarMetricsDefault];
    self.navigationBar.shadowImage = nil;
    self.navigationBar.translucent = NO;
    self.navigationBar = nil;
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  if (self.navigationController &&
      [self.navigationController
          isKindOfClass:[SettingsNavigationController class]]) {
    UIBarButtonItem* doneButton =
        [(SettingsNavigationController*)self.navigationController doneButton];
    self.navigationItem.rightBarButtonItem = doneButton;

    self.navigationBar = self.navigationController.navigationBar;
    [self.navigationBar setBackgroundImage:[[UIImage alloc] init]
                             forBarMetrics:UIBarMetricsDefault];
    self.navigationBar.shadowImage = [[UIImage alloc] init];
    self.navigationBar.translucent = YES;
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // Prevents potential recursive calls to `viewDidLayoutSubviews`.
  if (self.calculatingImageSize) {
    return;
  }
  // Rescale image here as on iPad the view height isn't correctly set before
  // subviews are laid out.
  self.calculatingImageSize = YES;
  self.imageView.image = [self createOrUpdateImage:self.imageView.image];
  self.calculatingImageSize = NO;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presenter passwordsInOtherAppsViewControllerDidDismiss];
  }
}

- (void)willMoveToParentViewController:(UIViewController*)parent {
  [super willMoveToParentViewController:parent];
  [self.navigationController setToolbarHidden:YES animated:YES];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.verticalSizeClass !=
      previousTraitCollection.verticalSizeClass) {
    self.imageView.image = [self createOrUpdateImage:self.imageView.image];
  }
}

#pragma mark - Accessors

- (UIScrollView*)scrollView {
  if (!_scrollView) {
    _scrollView = [[UIScrollView alloc] init];
    _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrollView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
    _scrollView.accessibilityIdentifier =
        kPasswordsInOtherAppsScrollViewAccessibilityIdentifier;
  }
  return _scrollView;
}

- (UIImageView*)imageView {
  if (!_imageView) {
    _imageView =
        [[UIImageView alloc] initWithImage:[self createOrUpdateImage:nil]];
    _imageView.clipsToBounds = YES;
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _imageView.accessibilityIdentifier =
        kPasswordsInOtherAppsImageAccessibilityIdentifier;
  }
  return _imageView;
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.numberOfLines = 0;

    UIFontTextStyle textStyle = UIFontTextStyleTitle2;
    UIFontDescriptor* descriptor =
        [UIFontDescriptor preferredFontDescriptorWithTextStyle:textStyle];
    UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                     weight:UIFontWeightBold];
    UIFontMetrics* fontMetrics = [UIFontMetrics metricsForTextStyle:textStyle];

    _titleLabel.font = [fontMetrics scaledFontForFont:font];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.text = self.titleText;
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.accessibilityIdentifier =
        kPasswordsInOtherAppsTitleAccessibilityIdentifier;
    _titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  }
  return _titleLabel;
}

- (UILabel*)subtitleLabel {
  if (!_subtitleLabel) {
    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _subtitleLabel.numberOfLines = 0;
    _subtitleLabel.textColor = [UIColor colorNamed:kGrey800Color];
    _subtitleLabel.text = self.subtitleText;
    _subtitleLabel.textAlignment = NSTextAlignmentCenter;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.adjustsFontForContentSizeCategory = YES;
    _subtitleLabel.accessibilityIdentifier =
        kPasswordsInOtherAppsSubtitleAccessibilityIdentifier;
  }
  return _subtitleLabel;
}

- (UIActivityIndicatorView*)spinner {
  if (!_spinner) {
    _spinner = GetMediumUIActivityIndicatorView();
    _spinner.hidesWhenStopped = YES;
    _spinner.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _spinner;
}

- (UIView*)specificContentView {
  if (!_specificContentView) {
    _specificContentView = [[UIView alloc] init];
    _specificContentView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _specificContentView;
}

- (UIButton*)actionButton {
  if (!_actionButton) {
    _actionButton = [[HighlightButton alloc] initWithFrame:CGRectZero];

    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        kButtonVerticalInsets, kButtonHorizontalMargin, kButtonVerticalInsets,
        kButtonHorizontalMargin);
    buttonConfiguration.background.backgroundColor =
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
    buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlueColor];

    UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    NSAttributedString* attributedTitle = [[NSAttributedString alloc]
        initWithString:self.actionString
            attributes:@{NSFontAttributeName : font}];
    buttonConfiguration.attributedTitle = attributedTitle;
    buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;
    _actionButton.configuration = buttonConfiguration;

    _actionButton.layer.cornerRadius = kPrimaryButtonCornerRadius;
    _actionButton.translatesAutoresizingMaskIntoConstraints = NO;
    _actionButton.pointerInteractionEnabled = YES;
    _actionButton.pointerStyleProvider =
        CreateOpaqueButtonPointerStyleProvider();
    _actionButton.accessibilityIdentifier =
        kPasswordsInOtherAppsActionAccessibilityIdentifier;

    [_actionButton addTarget:self
                      action:@selector(didTapActionButton)
            forControlEvents:UIControlEventTouchUpInside];
  }
  return _actionButton;
}

- (UIView*)turnOnInstructionView {
  if (!_turnOnInstructionView) {
    UIImage* icon = [UIImage imageNamed:@"settings"];
    NSArray<UIImage*>* icons =
        self.useShortInstruction ? nil : @[ icon, icon, icon, icon ];
    InstructionView* instruction =
        [[InstructionView alloc] initWithList:self.steps
                                        style:InstructionViewStyleGrayscale
                                        icons:icons];
    instruction.translatesAutoresizingMaskIntoConstraints = NO;
    UILayoutGuide* instructionLayoutGuide = [[UILayoutGuide alloc] init];

    _turnOnInstructionView = [[UIView alloc] init];
    _turnOnInstructionView.translatesAutoresizingMaskIntoConstraints = NO;
    [_turnOnInstructionView addSubview:self.spinner];
    [_turnOnInstructionView addLayoutGuide:instructionLayoutGuide];
    [_turnOnInstructionView addSubview:instruction];

    // Set constraints for top, leading, trailing edges and width.
    NSMutableArray<NSLayoutConstraint*>* constraints =
        [NSMutableArray arrayWithArray:@[
          [self.spinner.topAnchor
              constraintEqualToAnchor:_turnOnInstructionView.topAnchor],
          [self.spinner.centerXAnchor
              constraintEqualToAnchor:_turnOnInstructionView.centerXAnchor],
          [self.spinner.widthAnchor
              constraintEqualToAnchor:self.spinner.heightAnchor],
          [self.spinner.bottomAnchor
              constraintLessThanOrEqualToAnchor:_turnOnInstructionView
                                                    .bottomAnchor],
          [instructionLayoutGuide.topAnchor
              constraintEqualToAnchor:self.spinner.bottomAnchor
                             constant:kDefaultMargin],
          [instructionLayoutGuide.widthAnchor
              constraintEqualToAnchor:_turnOnInstructionView.widthAnchor],
          [instructionLayoutGuide.centerXAnchor
              constraintEqualToAnchor:_turnOnInstructionView.centerXAnchor],
          [instruction.topAnchor
              constraintGreaterThanOrEqualToAnchor:instructionLayoutGuide
                                                       .topAnchor],
          [instruction.bottomAnchor
              constraintEqualToAnchor:instructionLayoutGuide.bottomAnchor],
          [instruction.widthAnchor
              constraintEqualToAnchor:instructionLayoutGuide.widthAnchor],
          [instruction.centerXAnchor
              constraintEqualToAnchor:instructionLayoutGuide.centerXAnchor],
          [_turnOnInstructionView.topAnchor
              constraintEqualToAnchor:self.specificContentView.topAnchor],
          [_turnOnInstructionView.bottomAnchor
              constraintEqualToAnchor:self.specificContentView.bottomAnchor
                             constant:-kBottomMargin],
          [_turnOnInstructionView.centerXAnchor
              constraintEqualToAnchor:self.specificContentView.centerXAnchor],
          [_turnOnInstructionView.widthAnchor
              constraintEqualToAnchor:self.specificContentView.widthAnchor]
        ]];

    // Set constraints for bottom edge:
    // if the view will contain an action button, place action button at the
    // bottom and instruction right above it; otherwise, place the instruction
    // at the bottom.
    if (self.useShortInstruction) {
      [_turnOnInstructionView addSubview:self.actionButton];
      [constraints addObjectsFromArray:@[
        [self.actionButton.widthAnchor
            constraintEqualToAnchor:_turnOnInstructionView.widthAnchor],
        [self.actionButton.centerXAnchor
            constraintEqualToAnchor:_turnOnInstructionView.centerXAnchor],
        [self.actionButton.bottomAnchor
            constraintEqualToAnchor:_turnOnInstructionView.bottomAnchor
                           constant:-kBottomMargin],
        [instructionLayoutGuide.bottomAnchor
            constraintEqualToAnchor:self.actionButton.topAnchor
                           constant:-kDefaultMargin],
      ]];
    } else {
      [constraints addObjectsFromArray:@[
        [instructionLayoutGuide.bottomAnchor
            constraintEqualToAnchor:_turnOnInstructionView.bottomAnchor],
      ]];
    }
    _turnOnInstructionViewConstraints = constraints;
  }
  return _turnOnInstructionView;
}

- (UIView*)turnOffInstructionView {
  if (!_turnOffInstructionView) {
    UITextView* captionTextView = [self drawCaptionTextView];
    NSLog(@"%@", captionTextView.text);
    UIImage* checkmark = [[UIImage imageNamed:@"settings_safe_state"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    UIImageView* checkmarkView = [[UIImageView alloc] initWithImage:checkmark];
    checkmarkView.tintColor = [UIColor colorNamed:kGreenColor];
    checkmarkView.translatesAutoresizingMaskIntoConstraints = NO;

    _turnOffInstructionView = [[UIView alloc] init];
    _turnOffInstructionView.translatesAutoresizingMaskIntoConstraints = NO;
    [_turnOffInstructionView addSubview:captionTextView];
    [_turnOffInstructionView addSubview:checkmarkView];

    // Set constraints.
    self.turnOffInstructionViewConstraints = @[
      [captionTextView.topAnchor
          constraintEqualToAnchor:_turnOffInstructionView.topAnchor],
      [captionTextView.centerXAnchor
          constraintEqualToAnchor:_turnOffInstructionView.centerXAnchor],
      [captionTextView.widthAnchor
          constraintLessThanOrEqualToAnchor:_turnOffInstructionView
                                                .widthAnchor],
      [captionTextView.bottomAnchor
          constraintLessThanOrEqualToAnchor:_turnOffInstructionView
                                                .bottomAnchor],
      [checkmarkView.topAnchor
          constraintEqualToAnchor:captionTextView.bottomAnchor
                         constant:kCaptionTextViewOffset],
      [checkmarkView.centerXAnchor
          constraintEqualToAnchor:_turnOffInstructionView.centerXAnchor],
      [checkmarkView.widthAnchor
          constraintEqualToAnchor:checkmarkView.heightAnchor],
      [checkmarkView.bottomAnchor
          constraintLessThanOrEqualToAnchor:_turnOffInstructionView
                                                .bottomAnchor],
      [_turnOffInstructionView.topAnchor
          constraintEqualToAnchor:self.specificContentView.topAnchor],
      [_turnOffInstructionView.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [_turnOffInstructionView.widthAnchor
          constraintEqualToAnchor:self.specificContentView.widthAnchor],
      [_turnOffInstructionView.bottomAnchor
          constraintEqualToAnchor:self.specificContentView.bottomAnchor
                         constant:-kDefaultMargin]
    ];
  }
  return _turnOffInstructionView;
}

#pragma mark - PasswordsInOtherAppsConsumer

- (void)updateInstructionsWithCurrentPasswordAutoFillStatus {
  // Show instructions to turn off autoFill if auto-fill is enabled; show
  // instructions to turn on otherwise;
  PasswordAutoFillStatusManager* sharedManager =
      [PasswordAutoFillStatusManager sharedManager];
  BOOL shouldShowTurnOffInstructions =
      sharedManager.ready && sharedManager.autoFillEnabled;

  UIView* viewToRemove = shouldShowTurnOffInstructions
                             ? _turnOnInstructionView
                             : _turnOffInstructionView;
  if (viewToRemove != nil) {
    [viewToRemove removeFromSuperview];
  }
  [self.specificContentView addSubview:shouldShowTurnOffInstructions
                                           ? self.turnOffInstructionView
                                           : self.turnOnInstructionView];

  [NSLayoutConstraint
      activateConstraints:shouldShowTurnOffInstructions
                              ? self.turnOffInstructionViewConstraints
                              : self.turnOnInstructionViewConstraints];

  if (_spinner) {
    sharedManager.ready ? [_spinner stopAnimating] : [_spinner startAnimating];
  }
}

- (BOOL)useShortInstruction {
  return ios::provider::SupportShortenedInstructionForPasswordAutoFill() &&
         base::FeatureList::IsEnabled(
             kEnableShortenedPasswordAutoFillInstruction);
}

- (NSArray<NSString*>*)steps {
  if (self.useShortInstruction) {
    return @[
      l10n_util::GetNSString(
          IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_SHORTENED_STEP_1_IOS16),
      l10n_util::GetNSString(
          IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_SHORTENED_STEP_2)
    ];
  }
  return @[
    ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
        ? l10n_util::GetNSString(
              IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_1_IPAD)
        : l10n_util::GetNSString(
              IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_1_IPHONE),
    l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_2),
    l10n_util::GetNSString(
        IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_3_IOS16),
    l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_4)
  ];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return YES;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self didTapActionButton];
  return YES;
}

#pragma mark - Private

// Returns caption text that shows below the subtitle in turnOffInstructions.
- (UITextView*)drawCaptionTextView {
  NSString* text;
  text = l10n_util::GetNSString(
      IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_CAPTION_IOS16);
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kGrey600Color],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
  };

  NSDictionary* linkAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
    NSForegroundColorAttributeName : [UIColor colorNamed:kRedColor],
    NSLinkAttributeName : l10n_util::GetNSString(IDS_IOS_OPEN_SETTINGS),
  };

  UITextView* captionTextView = CreateUITextViewWithTextKit1();
  captionTextView.attributedText =
      AttributedStringFromStringWithLink(text, textAttributes, linkAttributes);
  captionTextView.editable = NO;
  captionTextView.scrollEnabled = NO;
  captionTextView.backgroundColor = self.view.backgroundColor;
  captionTextView.textContainerInset = UIEdgeInsetsMake(0, 0, 0, 0);
  captionTextView.textAlignment = NSTextAlignmentCenter;
  captionTextView.translatesAutoresizingMaskIntoConstraints = NO;
  captionTextView.adjustsFontForContentSizeCategory = YES;

  captionTextView.delegate = self;

  return captionTextView;
}

// Returns a new UIImage which is `sourceImage` resized to `newSize`.
// Returns `currentImage` if it is already at the correct size.
// Returns nil when the view should not show an image (iPhone landscape mode).
- (UIImage*)createOrUpdateImage:(UIImage*)currentImage {
  if (IsCompactHeight(self)) {
    return nil;
  }
  UIUserInterfaceStyle currentStyle =
      UITraitCollection.currentTraitCollection.userInterfaceStyle;
  CGSize newSize = [self computeBannerImageSize];
  if (CGSizeEqualToSize(newSize, currentImage.size) &&
      self.bannerStyle == currentStyle) {
    return currentImage;
  }
  self.bannerStyle = currentStyle;
  return ResizeImage([self bannerImage], newSize, ProjectionMode::kAspectFit);
}

// The banner image
- (UIImage*)bannerImage {
  return [UIImage imageNamed:self.bannerName];
}

// Computes banner's image size.
- (CGSize)computeBannerImageSize {
  CGFloat destinationHeight =
      roundf(self.view.bounds.size.height * kDefaultBannerMultiplier);
  CGFloat destinationWidth =
      roundf([self bannerImage].size.width / [self bannerImage].size.height *
             destinationHeight);
  CGSize newSize = CGSizeMake(destinationWidth, destinationHeight);
  return newSize;
}

// Selector of self.actionButton and link in caption text view.
- (void)didTapActionButton {
  [self.delegate openApplicationSettings];
}

@end
