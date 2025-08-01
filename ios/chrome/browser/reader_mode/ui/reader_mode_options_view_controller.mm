// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_view_controller.h"

#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_controls_view.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_mutator.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The horizontal inset for the main stack view.
constexpr CGFloat kMainStackViewInset = 16.0;

// Top and bottom padding.
constexpr CGFloat kTopPadding = 8;
constexpr CGFloat kBottomPadding = 54;

// The spacing between items in the main stack view.
constexpr CGFloat kMainStackViewSpacing = 16.0;

// The corner radius for the "Hide Reader Mode" button.
constexpr CGFloat kHideReaderModeButtonCornerRadius = 12.0;

// The minimum height for the "Hide Reader Mode" button.
constexpr CGFloat kHideReaderModeButtonMinHeight = 50.0;

// Opacity of the controls view when using a blur effect background.
constexpr CGFloat kBlurEffectBackgroundControlsOpacity = 0.95;

}  // namespace

@interface ReaderModeOptionsViewController ()

// Main stack view. Lazily created.
@property(nonatomic, readonly) UIStackView* mainStackView;

// Button to turn off Reader mode. Lazily created.
@property(nonatomic, readonly) UIButton* hideReaderModeButton;

@end

@implementation ReaderModeOptionsViewController

@synthesize controlsView = _controlsView;
@synthesize mainStackView = _mainStackView;
@synthesize hideReaderModeButton = _hideReaderModeButton;

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.title = l10n_util::GetNSString(IDS_IOS_READER_MODE_OPTIONS_TITLE);
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(hideReaderModeOptions)];
  self.navigationItem.rightBarButtonItem.accessibilityIdentifier =
      kReaderModeOptionsCloseButtonAccessibilityIdentifier;
  self.view.accessibilityIdentifier =
      kReaderModeOptionsViewAccessibilityIdentifier;

  // Add blurred background.
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterial];
  UIVisualEffectView* blurEffectView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  blurEffectView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:blurEffectView];
  AddSameConstraints(blurEffectView, self.view);

  UIView* mainStackView = self.mainStackView;
  [self.view addSubview:mainStackView];

  UILayoutGuide* safeAreaLayoutGuide = self.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [mainStackView.topAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor
                       constant:kTopPadding],
    [mainStackView.centerXAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.centerXAnchor],
    [mainStackView.widthAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.widthAnchor
                       constant:-2 * kMainStackViewInset]
  ]];

  [super viewDidLoad];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  __weak __typeof(self) weakSelf = self;
  [self.sheetPresentationController animateChanges:^{
    [weakSelf.sheetPresentationController invalidateDetents];
  }];
}

#pragma mark - Public properties

- (void)updateHideReaderModeButtonVisibility:(BOOL)visible {
  self.hideReaderModeButton.hidden = !visible;
}

#pragma mark - Public

- (CGFloat)resolveDetentValueForSheetPresentation:
    (id<UISheetPresentationControllerDetentResolutionContext>)context {
  CGFloat bottomPaddingAboveSafeArea =
      kBottomPadding - self.view.safeAreaInsets.bottom;
  return [self.mainStackView
             systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
             .height +
         kTopPadding +
         self.navigationController.navigationBar.frame.size.height +
         bottomPaddingAboveSafeArea;
}

#pragma mark - UI actions

- (void)hideReaderModeOptions {
  [self.readerModeOptionsHandler hideReaderModeOptions];
}

- (void)hideReaderMode {
  [self.mutator hideReaderMode];
}

#pragma mark - UI creation helpers

// Lazily creates and returns the main stack view.
- (UIStackView*)mainStackView {
  if (_mainStackView) {
    return _mainStackView;
  }

  UIStackView* mainStackView = [[UIStackView alloc] init];
  mainStackView.axis = UILayoutConstraintAxisVertical;
  mainStackView.spacing = kMainStackViewSpacing;
  mainStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [mainStackView addArrangedSubview:self.controlsView];
  [mainStackView addArrangedSubview:self.hideReaderModeButton];

  _mainStackView = mainStackView;
  return _mainStackView;
}

// Lazily creates and returns the controls view.
- (ReaderModeOptionsControlsView*)controlsView {
  if (_controlsView) {
    return _controlsView;
  }

  ReaderModeOptionsControlsView* controlsView =
      [[ReaderModeOptionsControlsView alloc] init];
  controlsView.translatesAutoresizingMaskIntoConstraints = NO;
  controlsView.backgroundColor =
      [[UIColor colorNamed:kGroupedSecondaryBackgroundColor]
          colorWithAlphaComponent:kBlurEffectBackgroundControlsOpacity];

  _controlsView = controlsView;
  return _controlsView;
}

// Returns the button to hide Reader mode.
- (UIButton*)hideReaderModeButton {
  if (_hideReaderModeButton) {
    return _hideReaderModeButton;
  }

  // Create button title attributed string.
  UIFontDescriptor* boldDescriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleBody]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  UIFont* fontAttribute = [UIFont fontWithDescriptor:boldDescriptor size:0.0];
  UIColor* textColor = [UIColor colorNamed:kSolidWhiteColor];
  NSDictionary* attributes = @{
    NSFontAttributeName : fontAttribute,
    NSForegroundColorAttributeName : textColor
  };
  NSMutableAttributedString* attributedTitle =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_READER_MODE_OPTIONS_HIDE_BUTTON_LABEL)
              attributes:attributes];

  // Create button configuration.
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration filledButtonConfiguration];
  configuration.baseBackgroundColor = [UIColor colorNamed:kBlue600Color];
  configuration.background.cornerRadius = kHideReaderModeButtonCornerRadius;
  configuration.attributedTitle = attributedTitle;

  // Create button.
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.configuration = configuration;
  button.maximumContentSizeCategory = UIContentSizeCategoryExtraExtraLarge;
  button.accessibilityIdentifier =
      kReaderModeOptionsTurnOffButtonAccessibilityIdentifier;
  [button addTarget:self
                action:@selector(hideReaderMode)
      forControlEvents:UIControlEventTouchUpInside];

  [button.heightAnchor
      constraintGreaterThanOrEqualToConstant:kHideReaderModeButtonMinHeight]
      .active = YES;

  _hideReaderModeButton = button;
  return _hideReaderModeButton;
}

@end
