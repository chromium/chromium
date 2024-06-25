// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Horizontal padding between all elements (except the previous/next buttons).
const CGFloat kPadding = 8;
// Horizontal padding between buttons and an adjacent superview edge in a
// Regular x Regular environment.
const CGFloat kIPadButtonEdgeSpacing = 17;
const CGFloat kButtonFontSize = 17;
const CGFloat kButtonSize = 44;
// Spacing between the increment/decrement buttons and the central divider.
const CGFloat kButtonDividerSpacing = 19;
// Width of the divider.
const CGFloat kDividerWidth = 1;
}

@interface TextZoomViewController ()

@property(nonatomic, assign) BOOL darkAppearance;

@property(nonatomic, strong) UIButton* closeButton;
@property(nonatomic, strong) UIButton* resetButton;

// Horizontal stack view to hold all the items that should be centered
// (increment/decrement, divider).
@property(nonatomic, strong) UIStackView* centerItemsStackView;
@property(nonatomic, strong) UIView* divider;
@property(nonatomic, strong) UIButton* incrementButton;
@property(nonatomic, strong) UIButton* decrementButton;

@end

@implementation TextZoomViewController

- (instancetype)init {
  return [super initWithNibName:nil bundle:nil];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:self.resetButton];
  [self.view addSubview:self.centerItemsStackView];
  [self.view addSubview:self.closeButton];

  const CGFloat buttonEdgeSpacing =
      ShouldShowCompactToolbar(self) ? kPadding : kIPadButtonEdgeSpacing;

  [NSLayoutConstraint activateConstraints:@[
    // Reset button.
    [self.resetButton.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [self.resetButton.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:buttonEdgeSpacing],
    // Use button intrinsic width.
    [self.resetButton.heightAnchor constraintEqualToConstant:kButtonSize],
    // Center items stack view.
    [self.centerItemsStackView.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [self.centerItemsStackView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.centerItemsStackView.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor],
    // Close Button.
    [self.closeButton.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [self.closeButton.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-buttonEdgeSpacing],
    // Use button intrinsic width.
    [self.closeButton.heightAnchor constraintEqualToConstant:kButtonSize],
  ]];

  [self.closeButton
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];
}

#pragma mark - Private property Accessors

// Creates and returns the close button.
- (UIButton*)closeButton {
  if (!_closeButton) {
    _closeButton = [self newButtonWithDefaultStyling];
    [_closeButton setTitle:l10n_util::GetNSString(IDS_DONE)
                  forState:UIControlStateNormal];
    _closeButton.accessibilityIdentifier = kTextZoomCloseButtonID;
    [_closeButton addTarget:self.commandHandler
                     action:@selector(closeTextZoom)
           forControlEvents:UIControlEventTouchUpInside];
  }
  return _closeButton;
}

// Creates and returns the reset button.
- (UIButton*)resetButton {
  if (!_resetButton) {
    _resetButton = [self newButtonWithDefaultStyling];
    [_resetButton setTitle:l10n_util::GetNSString(IDS_IOS_RESET_ZOOM)
                  forState:UIControlStateNormal];
    [_resetButton addTarget:self.zoomHandler
                     action:@selector(resetZoom)
           forControlEvents:UIControlEventTouchUpInside];
  }
  return _resetButton;
}

// Creates and returns the increment button.
- (UIButton*)incrementButton {
  if (!_incrementButton) {
    _incrementButton = [self newButtonWithDefaultStyling];
    UIImage* image = [UIImage imageNamed:@"text_zoom_zoom_in"];
    image.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_TEXT_ZOOM_ZOOM_IN);
    [_incrementButton setImage:image forState:UIControlStateNormal];
    [_incrementButton addTarget:self.zoomHandler
                         action:@selector(zoomIn)
               forControlEvents:UIControlEventTouchUpInside];
    [NSLayoutConstraint activateConstraints:@[
      [_incrementButton.heightAnchor constraintEqualToConstant:kButtonSize],
      [_incrementButton.widthAnchor
          constraintEqualToAnchor:_incrementButton.heightAnchor],
    ]];
  }
  return _incrementButton;
}

// Creates and returns the decrement button.
- (UIButton*)decrementButton {
  if (!_decrementButton) {
    _decrementButton = [self newButtonWithDefaultStyling];
    UIImage* image = [UIImage imageNamed:@"text_zoom_zoom_out"];
    image.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_TEXT_ZOOM_ZOOM_OUT);
    [_decrementButton setImage:image forState:UIControlStateNormal];
    [_decrementButton addTarget:self.zoomHandler
                         action:@selector(zoomOut)
               forControlEvents:UIControlEventTouchUpInside];
    [NSLayoutConstraint activateConstraints:@[
      [_decrementButton.heightAnchor constraintEqualToConstant:kButtonSize],
      [_decrementButton.widthAnchor
          constraintEqualToAnchor:_decrementButton.heightAnchor],
    ]];
  }
  return _decrementButton;
}

// Creates and returns the divider.
- (UIView*)divider {
  if (!_divider) {
    _divider = [[UIView alloc] init];
    _divider.translatesAutoresizingMaskIntoConstraints = NO;
    _divider.backgroundColor = [UIColor colorNamed:kSeparatorColor];
    [NSLayoutConstraint activateConstraints:@[
      [_divider.heightAnchor
          constraintEqualToAnchor:self.centerItemsStackView.heightAnchor
                         constant:-2 * kPadding],
      [_divider.widthAnchor constraintEqualToConstant:kDividerWidth],
    ]];
  }
  return _divider;
}

// Creates and returns the center items stack view.
- (UIStackView*)centerItemsStackView {
  if (!_centerItemsStackView) {
    _centerItemsStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      self.decrementButton, self.divider, self.incrementButton
    ]];
    _centerItemsStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _centerItemsStackView.spacing = kButtonDividerSpacing;
    _centerItemsStackView.alignment = UIStackViewAlignmentCenter;
  }
  return _centerItemsStackView;
}

#pragma mark - Property accessor helpers

- (UIButton*)newButtonWithDefaultStyling {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.font = [UIFont systemFontOfSize:kButtonFontSize];
  return button;
}

#pragma mark - TextZoomConsumer

- (void)setZoomInEnabled:(BOOL)enabled {
  self.incrementButton.enabled = enabled;
}

- (void)setZoomOutEnabled:(BOOL)enabled {
  self.decrementButton.enabled = enabled;
}

- (void)setResetZoomEnabled:(BOOL)enabled {
  self.resetButton.enabled = enabled;
}

@end
