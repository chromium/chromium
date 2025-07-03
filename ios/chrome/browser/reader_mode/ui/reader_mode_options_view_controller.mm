// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_view_controller.h"

#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_controls_view.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The horizontal inset for the main stack view.
constexpr CGFloat kMainStackViewHorizontalInset = 16.0;

// The spacing between items in the main stack view.
constexpr CGFloat kMainStackViewSpacing = 16.0;

// The corner radius for the "Hide Reader Mode" button.
constexpr CGFloat kHideReaderModeButtonCornerRadius = 12.0;

// The minimum height for the "Hide Reader Mode" button.
constexpr CGFloat kHideReaderModeButtonMinHeight = 50.0;

// The identifier for the custom content detent.
NSString* const kReaderModeOptionsViewControllerCustomDetentIdentifier =
    @"kReaderModeOptionsViewControllerCustomDetentIdentifier";

}  // namespace

@implementation ReaderModeOptionsViewController {
  UIStackView* _mainStackView;
}

@synthesize controlsView = _controlsView;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _controlsView = [self createControlsView];
  _mainStackView = [self createMainStackView];

  self.viewControllers = @[ [self createContentViewController] ];

  // Initialize custom content detent.
  UISheetPresentationControllerDetent* contentDetent =
      [self createCustomContentDetent];
  self.sheetPresentationController.detents = @[ contentDetent ];
  self.sheetPresentationController.largestUndimmedDetentIdentifier =
      contentDetent.identifier;
}

- (void)viewDidLayoutSubviews {
  __weak __typeof(self) weakSelf = self;
  [self.sheetPresentationController animateChanges:^{
    [weakSelf.sheetPresentationController invalidateDetents];
  }];
}

#pragma mark - Private

- (CGFloat)resolveDetentValueForSheetPresentation:
    (id<UISheetPresentationControllerDetentResolutionContext>)context {
  CGFloat mainStackHeight =
      [_mainStackView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
  return mainStackHeight + self.navigationBar.frame.size.height;
}

#pragma mark - UI actions

- (void)hideReaderModeOptions {
  [self.readerModeOptionsHandler hideReaderModeOptions];
}

- (void)hideReaderMode {
  // TODO(crbug.com/409941529): Hide Reader mode with mutator.
}

#pragma mark - UI creation helpers

// Returns the root view controller.
- (UIViewController*)createContentViewController {
  UIViewController* contentViewController = [[UIViewController alloc] init];
  contentViewController.title =
      l10n_util::GetNSString(IDS_IOS_READER_MODE_OPTIONS_TITLE);
  contentViewController.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                               target:self
                               action:@selector(hideReaderModeOptions)];
  contentViewController.navigationItem.rightBarButtonItem
      .accessibilityIdentifier =
      kReaderModeOptionsCloseButtonAccessibilityIdentifier;
  contentViewController.view.accessibilityIdentifier =
      kReaderModeOptionsViewAccessibilityIdentifier;
  contentViewController.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  [contentViewController.view addSubview:_mainStackView];

  UILayoutGuide* safeAreaLayoutGuide =
      contentViewController.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [safeAreaLayoutGuide.topAnchor
        constraintEqualToAnchor:_mainStackView.topAnchor],
    [safeAreaLayoutGuide.centerXAnchor
        constraintEqualToAnchor:_mainStackView.centerXAnchor],
    [safeAreaLayoutGuide.widthAnchor
        constraintEqualToAnchor:_mainStackView.widthAnchor
                       constant:2 * kMainStackViewHorizontalInset]
  ]];

  return contentViewController;
}

// Returns the main stack view.
- (UIStackView*)createMainStackView {
  UIStackView* mainStackView = [[UIStackView alloc] init];
  mainStackView.axis = UILayoutConstraintAxisVertical;
  mainStackView.spacing = kMainStackViewSpacing;
  mainStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [mainStackView addArrangedSubview:_controlsView];
  [mainStackView addArrangedSubview:[self createHideReaderModeButton]];

  return mainStackView;
}

// Returns the controls view.
- (ReaderModeOptionsControlsView*)createControlsView {
  ReaderModeOptionsControlsView* controlsView =
      [[ReaderModeOptionsControlsView alloc] init];
  controlsView.translatesAutoresizingMaskIntoConstraints = NO;
  controlsView.mutator = self.mutator;
  return controlsView;
}

// Returns the button to hide Reader mode.
- (UIButton*)createHideReaderModeButton {
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
  [button addTarget:self
                action:@selector(hideReaderMode)
      forControlEvents:UIControlEventTouchUpInside];

  [button.heightAnchor
      constraintGreaterThanOrEqualToConstant:kHideReaderModeButtonMinHeight]
      .active = YES;

  return button;
}

// Returns the custom content detent.
- (UISheetPresentationControllerDetent*)createCustomContentDetent {
  __weak __typeof(self) weakSelf = self;
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:
          kReaderModeOptionsViewControllerCustomDetentIdentifier
                        resolver:^CGFloat(
                            id<UISheetPresentationControllerDetentResolutionContext>
                                context) {
                          return [weakSelf
                              resolveDetentValueForSheetPresentation:context];
                        }];
}

@end
