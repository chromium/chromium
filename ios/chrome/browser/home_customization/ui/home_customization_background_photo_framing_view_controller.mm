// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_framing_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/ntp/ui_bundled/logo_vendor.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Constants for UI layout.
const CGFloat kButtonHeight = 50.0;
const CGFloat kButtonCornerRadius = 8.0;
const CGFloat kButtonSpacing = 12.0;
const CGFloat kContentPadding = 16.0;
const CGFloat kTopSpacingHeight = 5.0;
const CGFloat kOmniboxHeight = 50.0;
const CGFloat kOmniboxRadius = 22.5;
const CGFloat kPinchIconSize = 18.0;
const CGFloat kMaximumZoomScale = 5.0;
const CGFloat kSectionSpacing = 24.0;
const CGFloat kBottomHSpacingHeight = 5.0;
const CGFloat kLogoContainerWidth = 120.0;
const CGFloat kLogoContainerHeight = 40.0;
const CGFloat kCenterStackSpacing = 4.0;
}  // namespace

@interface HomeCustomizationImageFramingViewController () <
    UIScrollViewDelegate> {
  // The original image to be framed.
  UIImage* _originalImage;
  // Logo vendor for displaying Google logo/doodle.
  id<LogoVendor> _logoVendor;
  // Image view displaying the image.
  UIImageView* _imageView;
  // Bottom section container.
  UIView* _bottomSection;
  // Scroll view for zooming and panning.
  UIScrollView* _scrollView;
}

@end

@implementation HomeCustomizationImageFramingViewController

#pragma mark - Initialization

- (instancetype)initWithImage:(UIImage*)image
                   logoVendor:(id<LogoVendor>)logoVendor {
  CHECK(image);
  CHECK(logoVendor);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _originalImage = image;
    _logoVendor = logoVendor;
    // Set fullscreen presentation.
    self.modalPresentationStyle = UIModalPresentationOverFullScreen;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kTextPrimaryColor];

  [self setupScrollView];
  [self setupImageView];
  [self setupTopSection];
  [self setupBottomSection];
  [self setupCenterContainer];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self updateMinimumZoomScale];
  [self centerImageView];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:nil
                      completion:^(
                          id<UIViewControllerTransitionCoordinatorContext>
                              context) {
                        [weakSelf updateMinimumZoomScale];
                        [weakSelf centerImageView];
                      }];
}

#pragma mark - UIScrollViewDelegate

- (UIView*)viewForZoomingInScrollView:(UIScrollView*)scrollView {
  return _imageView;
}

- (void)scrollViewDidZoom:(UIScrollView*)scrollView {
  [self centerImageView];
}

#pragma mark - Private

// Sets up the scroll view for image zooming and panning.
- (void)setupScrollView {
  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView.delegate = self;
  _scrollView.showsVerticalScrollIndicator = NO;
  _scrollView.showsHorizontalScrollIndicator = NO;
  _scrollView.decelerationRate = UIScrollViewDecelerationRateFast;
  _scrollView.minimumZoomScale = 1.0;
  _scrollView.maximumZoomScale = kMaximumZoomScale;
  _scrollView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  [self.view addSubview:_scrollView];

  // Pin scroll view to view edges.
  AddSameConstraints(_scrollView, self.view);
}

// Configures the image view and adds it to the scroll view.
- (void)setupImageView {
  _imageView = [[UIImageView alloc] initWithImage:_originalImage];
  _imageView.contentMode = UIViewContentModeScaleAspectFit;
  [_scrollView addSubview:_imageView];

  _scrollView.contentSize = _imageView.frame.size;
}

// Creates the top section with logo and omnibox.
- (void)setupTopSection {
  // Top section container.
  UIStackView* topSection = [[UIStackView alloc] init];
  topSection.axis = UILayoutConstraintAxisVertical;
  topSection.alignment = UIStackViewAlignmentCenter;
  topSection.spacing = kSectionSpacing;
  topSection.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:topSection];

  // Add logo vendor view.
  if (_logoVendor) {
    // Get logo view and configure it.
    UIView* logoView = _logoVendor.view;
    logoView.translatesAutoresizingMaskIntoConstraints = NO;
    _logoVendor.showingLogo = YES;
    _logoVendor.usesMonochromeLogo = YES;
    logoView.tintColor = [UIColor colorNamed:kSolidWhiteColor];
    [topSection addArrangedSubview:logoView];

    [NSLayoutConstraint activateConstraints:@[
      [logoView.widthAnchor constraintEqualToConstant:kLogoContainerWidth],
      [logoView.heightAnchor constraintEqualToConstant:kLogoContainerHeight]
    ]];
  }

  // Omnibox view.
  UIView* omniboxView = [[UIView alloc] init];
  omniboxView.backgroundColor = [UIColor whiteColor];
  omniboxView.layer.cornerRadius = kOmniboxRadius;
  omniboxView.translatesAutoresizingMaskIntoConstraints = NO;
  [topSection addArrangedSubview:omniboxView];

  [NSLayoutConstraint activateConstraints:@[
    // Top section container.
    [topSection.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kContentPadding + kTopSpacingHeight +
                                kSectionSpacing],
    [topSection.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                             constant:kContentPadding],
    [topSection.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                              constant:-kContentPadding],

    // Omnibox view.
    [omniboxView.widthAnchor constraintEqualToAnchor:topSection.widthAnchor],
    [omniboxView.heightAnchor constraintEqualToConstant:kOmniboxHeight]
  ]];
}

// Creates the bottom section with save and cancel buttons.
- (void)setupBottomSection {
  // Bottom section container using stack view.
  _bottomSection = [[UIStackView alloc] init];
  UIStackView* bottomStack = (UIStackView*)_bottomSection;
  bottomStack.axis = UILayoutConstraintAxisVertical;
  bottomStack.alignment = UIStackViewAlignmentFill;
  bottomStack.spacing = kButtonSpacing;
  bottomStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_bottomSection];

  // Save button.
  UIButtonConfiguration* saveConfig =
      [UIButtonConfiguration filledButtonConfiguration];
  saveConfig.title = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_FRAMING_VIEW_SAVE_BUTTON_LABEL);
  saveConfig.baseBackgroundColor = [UIColor colorNamed:kBlueColor];
  saveConfig.baseForegroundColor = [UIColor colorNamed:kSolidWhiteColor];
  saveConfig.cornerStyle = UIButtonConfigurationCornerStyleFixed;
  saveConfig.background.cornerRadius = kButtonCornerRadius;

  UIButton* saveButton = [UIButton buttonWithConfiguration:saveConfig
                                             primaryAction:nil];
  saveButton.translatesAutoresizingMaskIntoConstraints = NO;
  [saveButton addTarget:self
                 action:@selector(saveButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  [bottomStack addArrangedSubview:saveButton];

  // Cancel button.
  UIButtonConfiguration* cancelConfig =
      [UIButtonConfiguration filledButtonConfiguration];
  cancelConfig.title = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_FRAMING_VIEW_CANCEL_BUTTON_LABEL);
  cancelConfig.baseBackgroundColor = [UIColor colorNamed:kSolidWhiteColor];
  cancelConfig.baseForegroundColor = [UIColor colorNamed:kBlueColor];
  cancelConfig.cornerStyle = UIButtonConfigurationCornerStyleFixed;
  cancelConfig.background.cornerRadius = kButtonCornerRadius;

  UIButton* cancelButton = [UIButton buttonWithConfiguration:cancelConfig
                                               primaryAction:nil];
  cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  [cancelButton addTarget:self
                   action:@selector(cancelButtonTapped)
         forControlEvents:UIControlEventTouchUpInside];
  [bottomStack addArrangedSubview:cancelButton];

  // Constraints for stack view and button heights.
  [NSLayoutConstraint activateConstraints:@[
    // Bottom section container.
    [_bottomSection.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-(kContentPadding + kBottomHSpacingHeight +
                                  kButtonSpacing)],
    [_bottomSection.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kContentPadding],
    [_bottomSection.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kContentPadding],

    // Button heights.
    [saveButton.heightAnchor constraintEqualToConstant:kButtonHeight],
    [cancelButton.heightAnchor constraintEqualToConstant:kButtonHeight]
  ]];
}

// Creates the center container with pinch instruction.
- (void)setupCenterContainer {
  // Center container for pinch instruction using horizontal stack view.
  UIStackView* centerContainer = [[UIStackView alloc] init];
  centerContainer.alignment = UIStackViewAlignmentCenter;
  centerContainer.spacing = kCenterStackSpacing;
  centerContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:centerContainer];

  // Pinch icon.
  UIImage* pinchIcon = DefaultSymbolWithPointSize(kCropSymbol, kPinchIconSize);
  UIImageView* pinchIconView = [[UIImageView alloc] initWithImage:pinchIcon];
  pinchIconView.tintColor = [UIColor colorNamed:kSolidWhiteColor];
  pinchIconView.contentMode = UIViewContentModeScaleAspectFit;
  pinchIconView.translatesAutoresizingMaskIntoConstraints = NO;
  [centerContainer addArrangedSubview:pinchIconView];

  // Add size constraints for the icon.
  [NSLayoutConstraint activateConstraints:@[
    [pinchIconView.widthAnchor constraintEqualToConstant:kPinchIconSize],
    [pinchIconView.heightAnchor constraintEqualToConstant:kPinchIconSize]
  ]];

  // Pinch label.
  UILabel* pinchLabel = [[UILabel alloc] init];
  pinchLabel.text = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_FRAMING_VIEW_PINCH_TO_RESIZE);
  pinchLabel.textColor = [UIColor colorNamed:kSolidWhiteColor];
  pinchLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  pinchLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [centerContainer addArrangedSubview:pinchLabel];

  // Set up constraints for center container.
  [NSLayoutConstraint activateConstraints:@[
    [centerContainer.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [centerContainer.bottomAnchor
        constraintEqualToAnchor:_bottomSection.topAnchor
                       constant:-kContentPadding]
  ]];
}

// Updates the minimum zoom scale to fill the screen.
- (void)updateMinimumZoomScale {
  CGSize scrollViewSize = _scrollView.bounds.size;
  CGSize imageSize = _originalImage.size;

  // Calculate the scale needed to fill the screen.
  CGFloat widthScale = scrollViewSize.width / imageSize.width;
  CGFloat heightScale = scrollViewSize.height / imageSize.height;
  CGFloat minimumScale = MAX(widthScale, heightScale);

  _scrollView.minimumZoomScale = minimumScale;
  _scrollView.zoomScale = minimumScale;
}

// Centers the image view within the scroll view bounds.
- (void)centerImageView {
  CGSize scrollViewSize = _scrollView.bounds.size;
  CGSize contentSize = _scrollView.contentSize;

  CGFloat offsetX = (scrollViewSize.width > contentSize.width)
                        ? (scrollViewSize.width - contentSize.width) * 0.5
                        : 0;
  CGFloat offsetY = (scrollViewSize.height > contentSize.height)
                        ? (scrollViewSize.height - contentSize.height) * 0.5
                        : 0;

  _imageView.center = CGPointMake(contentSize.width * 0.5 + offsetX,
                                  contentSize.height * 0.5 + offsetY);
}

// Handles cancel button tap and notifies delegate.
- (void)cancelButtonTapped {
  [self.delegate imageFramingViewControllerDidCancel:self];
}

// Handles save button tap functionality.
- (void)saveButtonTapped {
  // TODO(crbug.com/421925583): Implement save button and integrate with
  // services.
}

@end
