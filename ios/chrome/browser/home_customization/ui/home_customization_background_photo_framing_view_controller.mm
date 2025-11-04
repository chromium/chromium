// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_framing_view_controller.h"

#import <cmath>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_accessibility_identifiers.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_framing_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_search_engine_logo_mediator_provider.h"
#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/shared/ui/elements/passthrough_stack_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Constants for UI layout.
const CGFloat kButtonHeight = 50.0;
const CGFloat kButtonCornerRadius = 8.0;
const CGFloat kButtonSpacing = 12.0;
const CGFloat kButtonMaxWidth = 500;
const CGFloat kSaveButtonImagePadding = 4;
const CGFloat kContentPadding = 16.0;
const CGFloat kTopSpacingHeight = 5.0;
const CGFloat kPinchIconSize = 18.0;
const CGFloat kMaximumZoomScale = 5.0;
const CGFloat kSectionSpacing = 24.0;
const CGFloat kBottomHSpacingHeight = 5.0;
const CGFloat kLogoContainerWidth = 120.0;
const CGFloat kLogoContainerHeight = 40.0;
const CGFloat kCenterStackSpacing = 4.0;
const CGFloat kGradientEndPoint = 0.62;
const CGFloat kGradientSpacingAboveInstructions = 150;
}  // namespace

@interface HomeCustomizationImageFramingViewController () <
    UIScrollViewDelegate> {
  // The original image to be framed.
  UIImage* _originalImage;
  // Image view displaying the image.
  UIImageView* _imageView;
  // Bottom section container.
  UIView* _bottomSection;
  // The save button.
  UIButton* _saveButton;
  // Scroll view for zooming and panning.
  UIScrollView* _scrollView;
  // Stack view to hold the pinch instruction views (label and icon).
  UIStackView* _pinchInstructionsView;
  // Constraint for the width of the fake omnibox.
  NSLayoutConstraint* _omniboxWidthConstraint;
  // Whether or not the view has laid out subviews for the first time. Used
  // because some scroll view properties can't be controlled via constraint
  // and must wait for after all the subview sizes have determined.
  BOOL _hasLaidOutSubviews;
}

@end

@implementation HomeCustomizationImageFramingViewController

#pragma mark - Initialization

- (instancetype)initWithImage:(UIImage*)image {
  CHECK(image);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _originalImage = image;
    // Set fullscreen presentation.
    self.modalPresentationStyle = UIModalPresentationOverFullScreen;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = UIColor.blackColor;
  self.view.accessibilityIdentifier =
      kPhotoFramingMainViewAccessibilityIdentifier;

  [self setupScrollView];
  [self setupImageView];
  [self setupTopSection];
  [self setupBottomSection];
  [self setupPinchInstructions];
  [self setupGradientView];
}

- (void)viewIsAppearing:(BOOL)animated {
  [super viewIsAppearing:animated];

  [self updateOmniboxWidth];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  [self updateZoomScaleBounds];
  if (!_hasLaidOutSubviews) {
    // For the first appearance, start the zoom at 1, unless the image is too
    // small for that.
    _scrollView.zoomScale = MIN(1, _scrollView.minimumZoomScale);
    // Start the flow with the center of the image in the center of the view.
    [self setScrollableContentCenterRatio:CGPointMake(0.5, 0.5)];
  }

  _hasLaidOutSubviews = YES;
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  CGRect visibleContent =
      CGRectMake(_scrollView.contentOffset.x, _scrollView.contentOffset.y,
                 _scrollView.bounds.size.width, _scrollView.bounds.size.height);

  CGPoint centerRatio = CGPointMake(
      CGRectGetMidX(visibleContent) / _scrollView.contentSize.width,
      CGRectGetMidY(visibleContent) / _scrollView.contentSize.height);

  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf performViewWillTransitionToSizeAnimationsKeepingCenterRatio:
                      centerRatio];
      }
                      completion:nil];
}

#pragma mark - UIScrollViewDelegate

- (UIView*)viewForZoomingInScrollView:(UIScrollView*)scrollView {
  return _imageView;
}

#pragma mark - Private

// Sets up the scroll view for image zooming and panning.
- (void)setupScrollView {
  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView.delegate = self;
  _scrollView.showsVerticalScrollIndicator = NO;
  _scrollView.showsHorizontalScrollIndicator = NO;
  _scrollView.alwaysBounceVertical = YES;
  _scrollView.alwaysBounceHorizontal = YES;
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
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageView.contentMode = UIViewContentModeScaleAspectFit;
  [_scrollView addSubview:_imageView];

  AddSameConstraints(_imageView, _scrollView.contentLayoutGuide);
}

// Creates the top section with logo and omnibox.
- (void)setupTopSection {
  // Top section container.
  PassthroughStackView* topSection = [[PassthroughStackView alloc] init];
  topSection.axis = UILayoutConstraintAxisVertical;
  topSection.alignment = UIStackViewAlignmentCenter;
  topSection.spacing = kSectionSpacing;
  topSection.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:topSection];

  // Add logo vendor view.
  // Get logo view and configure it.
  SearchEngineLogoMediator* searchEngineLogoMediator =
      [self.searchEngineLogoMediatorProvider
          provideSearchEngineLogoMediatorForKey:@"kUserUploaded"];
  UIView* logoView = searchEngineLogoMediator.view;
  logoView.translatesAutoresizingMaskIntoConstraints = NO;

  searchEngineLogoMediator.usesMonochromeLogo = YES;
  // Real logo is always white, even in dark mode.
  logoView.tintColor = UIColor.whiteColor;
  [topSection addArrangedSubview:logoView];

  [NSLayoutConstraint activateConstraints:@[
    [logoView.widthAnchor constraintEqualToConstant:kLogoContainerWidth],
    [logoView.heightAnchor constraintEqualToConstant:kLogoContainerHeight]
  ]];

  // Omnibox view.
  UIVisualEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterial];
  UIView* omniboxView = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  omniboxView.clipsToBounds = YES;
  omniboxView.translatesAutoresizingMaskIntoConstraints = NO;
  [topSection addArrangedSubview:omniboxView];

  CGFloat omniboxHeight = content_suggestions::FakeOmniboxHeight();
  omniboxView.layer.cornerRadius = omniboxHeight / 2;

  _omniboxWidthConstraint =
      [omniboxView.widthAnchor constraintEqualToConstant:0],

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
    _omniboxWidthConstraint,
    [omniboxView.heightAnchor constraintEqualToConstant:omniboxHeight]
  ]];
}

// Creates the bottom section with save and cancel buttons.
- (void)setupBottomSection {
  // Bottom section container using stack view.
  PassthroughStackView* bottomStack = [[PassthroughStackView alloc] init];
  _bottomSection = bottomStack;
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
  saveConfig.baseForegroundColor = [UIColor colorNamed:kSolidButtonTextColor];
  saveConfig.cornerStyle = UIButtonConfigurationCornerStyleFixed;
  saveConfig.background.cornerRadius = kButtonCornerRadius;
  // Configuration options for the activity indicator.
  saveConfig.imagePlacement = NSDirectionalRectEdgeTrailing;
  saveConfig.imagePadding = kSaveButtonImagePadding;

  _saveButton = [UIButton buttonWithConfiguration:saveConfig primaryAction:nil];
  _saveButton.translatesAutoresizingMaskIntoConstraints = NO;
  _saveButton.accessibilityIdentifier =
      kPhotoFramingViewSaveButtonAccessibilityIdentifier;
  [_saveButton addTarget:self
                  action:@selector(saveButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  _saveButton.configurationUpdateHandler = ^(UIButton* button) {
    UIButtonConfiguration* configuration = button.configuration;
    // When disabled, set the background color  explicitly, as by default,
    // UIButton transforms the baseBackgroundColor to be semi-transparent which
    // does not look good with an image in the background.
    if (button.isEnabled) {
      configuration.background.backgroundColor = nil;
    } else {
      configuration.background.backgroundColor =
          [UIColor colorNamed:kDisabledTintColor];
    }
    button.configuration = configuration;
  };

  [bottomStack addArrangedSubview:_saveButton];

  // Cancel button.
  UIButtonConfiguration* cancelConfig =
      [UIButtonConfiguration filledButtonConfiguration];
  cancelConfig.title = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_FRAMING_VIEW_CANCEL_BUTTON_LABEL);
  cancelConfig.baseBackgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
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

  // Use two non-required constraints to constrain the width normally, but also
  // add a max width when the view gets too wide (e.g. landscape).
  NSLayoutConstraint* buttonMaxWidthConstraint =
      [_bottomSection.widthAnchor constraintEqualToConstant:kButtonMaxWidth];
  NSLayoutConstraint* buttonWidthPaddingConstraint =
      [_bottomSection.widthAnchor constraintEqualToAnchor:self.view.widthAnchor
                                                 constant:-2 * kContentPadding];
  buttonMaxWidthConstraint.priority = UILayoutPriorityDefaultHigh;
  buttonWidthPaddingConstraint.priority = UILayoutPriorityDefaultHigh;

  // Constraints for stack view and button heights.
  [NSLayoutConstraint activateConstraints:@[
    // Bottom section container.
    [_bottomSection.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-(kContentPadding + kBottomHSpacingHeight +
                                  kButtonSpacing)],
    buttonMaxWidthConstraint, buttonWidthPaddingConstraint,
    [_bottomSection.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],

    // Button heights.
    [_saveButton.heightAnchor constraintEqualToConstant:kButtonHeight],
    [cancelButton.heightAnchor constraintEqualToConstant:kButtonHeight]
  ]];
}

// Creates the pinch instructions view.
- (void)setupPinchInstructions {
  // Pinch instruction container view using horizontal stack view.
  _pinchInstructionsView = [[UIStackView alloc] init];
  _pinchInstructionsView.alignment = UIStackViewAlignmentCenter;
  _pinchInstructionsView.spacing = kCenterStackSpacing;
  _pinchInstructionsView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_pinchInstructionsView];

  // Pinch icon.
  UIImage* pinchIcon = DefaultSymbolWithPointSize(kCropSymbol, kPinchIconSize);
  UIImageView* pinchIconView = [[UIImageView alloc] initWithImage:pinchIcon];
  pinchIconView.tintColor = UIColor.whiteColor;
  pinchIconView.contentMode = UIViewContentModeScaleAspectFit;
  pinchIconView.translatesAutoresizingMaskIntoConstraints = NO;
  [_pinchInstructionsView addArrangedSubview:pinchIconView];

  // Add size constraints for the icon.
  [NSLayoutConstraint activateConstraints:@[
    [pinchIconView.widthAnchor constraintEqualToConstant:kPinchIconSize],
    [pinchIconView.heightAnchor constraintEqualToConstant:kPinchIconSize]
  ]];

  // Pinch label.
  UILabel* pinchLabel = [[UILabel alloc] init];
  pinchLabel.text = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_FRAMING_VIEW_PINCH_TO_RESIZE);
  pinchLabel.textColor = UIColor.whiteColor;
  pinchLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  pinchLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [_pinchInstructionsView addArrangedSubview:pinchLabel];

  // Set up constraints for center container.
  [NSLayoutConstraint activateConstraints:@[
    [_pinchInstructionsView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_pinchInstructionsView.bottomAnchor
        constraintEqualToAnchor:_bottomSection.topAnchor
                       constant:-kContentPadding]
  ]];
}

// Performs any necessary updates for when the view changes size (e.g. during a
// rotation). Keeps the center of the displayed content in the scroll view as
// constant as possible. The center is defined as a ratio between 0 and 1 on
// each axis, as the actual center coordinates may change if the scroll
// parameters change due to rotation.
- (void)performViewWillTransitionToSizeAnimationsKeepingCenterRatio:
    (CGPoint)centerRatio {
  [self updateZoomScaleBounds];
  [self setScrollableContentCenterRatio:centerRatio];
  [self updateOmniboxWidth];
}

// Configures the gradient view behind the bottom part of the screen.
- (void)setupGradientView {
  UIColor* startColor = [UIColor.blackColor colorWithAlphaComponent:0];
  UIColor* endColor = [UIColor.blackColor colorWithAlphaComponent:0.6];
  UIView* gradientView = [[GradientView alloc]
      initWithStartColor:startColor
                endColor:endColor
              startPoint:CGPointMake(0, 0)
                endPoint:CGPointMake(0, kGradientEndPoint)];
  gradientView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view insertSubview:gradientView aboveSubview:_scrollView];
  AddSameConstraintsToSides(
      gradientView, self.view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
  [_pinchInstructionsView.topAnchor
      constraintEqualToAnchor:gradientView.topAnchor
                     constant:kGradientSpacingAboveInstructions]
      .active = YES;
}

// Updates the width of the fake omnibox based on the current view width.
- (void)updateOmniboxWidth {
  CGFloat contentWidth =
      std::fmax(0, self.view.bounds.size.width - self.view.safeAreaInsets.left -
                       self.view.safeAreaInsets.right);
  if (contentWidth == 0) {
    return;
  }

  _omniboxWidthConstraint.constant =
      content_suggestions::SearchFieldWidth(contentWidth, self.traitCollection);
}

// Updates the minimum zoom scale to fill the screen.
- (void)updateZoomScaleBounds {
  CGSize scrollViewSize = _scrollView.bounds.size;
  CGSize imageSize = _originalImage.size;

  // Calculate the scale needed to fill the screen.
  CGFloat widthScale = scrollViewSize.width / imageSize.width;
  // Ensure that the image will fill the screen. Due to floating point
  // imprecision, sometimes the image would be slightly smaller than the screen,
  // so fix that here.
  if (widthScale * imageSize.width < scrollViewSize.width) {
    widthScale = std::nextafter(widthScale, CGFLOAT_MAX);
  }
  CGFloat heightScale = scrollViewSize.height / imageSize.height;
  if (heightScale * imageSize.height < scrollViewSize.height) {
    heightScale = std::nextafter(heightScale, CGFLOAT_MAX);
  }

  CGFloat minimumScale = MAX(widthScale, heightScale);

  _scrollView.minimumZoomScale = minimumScale;
  // Always allow some zooming, even if the image is very small and thus already
  // very zoomed in.
  _scrollView.maximumZoomScale = MAX(kMaximumZoomScale, minimumScale + 2);
  // Re-setting the zoom scale will factor any new min/max zoom scale into the
  // actual final value.
  _scrollView.zoomScale = _scrollView.zoomScale;
}

// Sets the displayed center of the scroll view to as close to `centerRatio` as
// possible. The center is defined as a ratio between 0 and 1 on each axis, as
// the actual center coordinates may change if the scroll parameters change due
// to rotation.
- (void)setScrollableContentCenterRatio:(CGPoint)centerRatio {
  CGPoint newCenter =
      CGPointMake(_scrollView.contentSize.width * centerRatio.x,
                  _scrollView.contentSize.height * centerRatio.y);

  CGPoint desiredContentOffset =
      CGPointMake(newCenter.x - _scrollView.bounds.size.width / 2,
                  newCenter.y - _scrollView.bounds.size.height / 2);

  // Actual content offet may need to be adjusted so the image doesn't go beyond
  // the scroll view.
  CGPoint newContentOffset = CGPointMake(
      std::clamp<CGFloat>(
          desiredContentOffset.x, 0,
          _scrollView.contentSize.width - _scrollView.bounds.size.width),
      std::clamp<CGFloat>(
          desiredContentOffset.y, 0,
          _scrollView.contentSize.height - _scrollView.bounds.size.height));

  _scrollView.contentOffset = newContentOffset;
}

// Handles cancel button tap and notifies delegate.
- (void)cancelButtonTapped {
  [self.delegate imageFramingViewControllerDidCancel:self];
}

- (void)saveButtonTapped {
  UIButtonConfiguration* configuration = _saveButton.configuration;
  configuration.showsActivityIndicator = YES;
  _saveButton.configuration = configuration;
  _saveButton.enabled = NO;
  if (!_mutator) {
    [self.delegate imageFramingViewControllerDidCancel:self];
    return;
  }
  HomeCustomizationFramingCoordinates* framingCoordinates =
      [self calculateFramingCoordinates];

  __weak __typeof(self) weakSelf = self;
  [_mutator saveImage:_originalImage
      withFramingCoordinates:framingCoordinates
                  completion:base::BindOnce(^(BOOL success) {
                    [weakSelf handleImageSave:success];
                  })];
}

// Handles image save completion.
- (void)handleImageSave:(BOOL)success {
  if (!success) {
    UIButtonConfiguration* configuration = _saveButton.configuration;
    configuration.showsActivityIndicator = NO;
    _saveButton.configuration = configuration;
    _saveButton.enabled = YES;
    return;
  }

  [self.delegate imageFramingViewControllerDidSucceed:self];
}

// Calculates the framing coordinates for the visible area in content
// coordinates.
- (HomeCustomizationFramingCoordinates*)calculateFramingCoordinates {
  // Get the visible area in scroll view content coordinates.
  CGRect visibleContentRect =
      CGRectMake(_scrollView.contentOffset.x, _scrollView.contentOffset.y,
                 _scrollView.bounds.size.width, _scrollView.bounds.size.height);

  // Convert from scroll view content coordinates to original image coordinates.
  CGFloat scale = _originalImage.size.width / _imageView.frame.size.width;

  CGRect visibleRectInOriginal = CGRectMake(
      visibleContentRect.origin.x * scale, visibleContentRect.origin.y * scale,
      visibleContentRect.size.width * scale,
      visibleContentRect.size.height * scale);

  // Translate the rect, keeping the same size, to make sure it's inside the
  // bounds of the original image.
  visibleRectInOriginal.origin.x =
      std::clamp<CGFloat>(visibleRectInOriginal.origin.x, 0,
                          std::fmax(0, _originalImage.size.width -
                                           visibleRectInOriginal.size.width));
  visibleRectInOriginal.origin.y =
      std::clamp<CGFloat>(visibleRectInOriginal.origin.y, 0,
                          std::fmax(0, _originalImage.size.height -
                                           visibleRectInOriginal.size.height));

  return [[HomeCustomizationFramingCoordinates alloc]
      initWithVisibleRect:visibleRectInOriginal];
}

@end
