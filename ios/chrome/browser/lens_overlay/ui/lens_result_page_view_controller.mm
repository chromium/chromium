// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/components/ui_util/dynamic_type_util.h"

namespace {

/// Top padding for the view content.
const CGFloat kViewTopPadding = 19;

/// Width of the back button.
const CGFloat kBackButtonWidth = 44;
/// Size of the back button.
const CGFloat kBackButtonSize = 24;

/// Minimum leading and trailing padding for the omnibox container.
const CGFloat kOmniboxContainerHorizontalPadding = 12;
/// Minimum height of the omnibox container.
const CGFloat kOmniboxContainerMinimumHeight = 42;
/// Minimum padding between the top of the view and the top of the web
/// container.
const CGFloat kWebContainerTopPadding = 8;

}  // namespace

@interface LensResultPageViewController ()

/// Web view in `_webViewContainer`.
@property(nonatomic, strong) UIView* webView;

@end

@implementation LensResultPageViewController {
  /// Back button.
  UIButton* _backButton;
  /// Container for the omnibox.
  UIButton* _omniboxContainer;
  /// StackView for the `_backButton` and `_omniboxContainer`.
  UIStackView* _horizontalStackView;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _webViewContainer = [[UIView alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  CHECK(self.webViewContainer, kLensOverlayNotFatalUntil);
  // Webview container.
  self.webViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.webViewContainer];

  // Back Button.
  _backButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _backButton.translatesAutoresizingMaskIntoConstraints = NO;
  _backButton.hidden = YES;
  UIImage* image =
      DefaultSymbolWithPointSize(kChevronBackwardSymbol, kBackButtonSize);
  [_backButton setImage:image forState:UIControlStateNormal];
  [_backButton addTarget:self
                  action:@selector(didTapBackButton:)
        forControlEvents:UIControlEventTouchUpInside];

  // Omnibox container.
  _omniboxContainer = [[UIButton alloc] init];
  _omniboxContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _omniboxContainer.backgroundColor = [UIColor colorNamed:kGrey200Color];
  _omniboxContainer.layer.cornerRadius = 21;
  [_omniboxContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];
  [_omniboxContainer addTarget:self
                        action:@selector(didTapOmniboxContainer:)
              forControlEvents:UIControlEventTouchUpInside];

  // Horizontal stack view.
  _horizontalStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _backButton, _omniboxContainer ]];
  _horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  _horizontalStackView.distribution = UIStackViewDistributionFill;
  [self.view addSubview:_horizontalStackView];

  NSLayoutConstraint* omniboxLeadingConstraint =
      [_omniboxContainer.leadingAnchor
          constraintEqualToAnchor:self.view.leadingAnchor
                         constant:kOmniboxContainerHorizontalPadding];
  omniboxLeadingConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [_horizontalStackView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kViewTopPadding],
    [_backButton.widthAnchor constraintEqualToConstant:kBackButtonWidth],
    [_horizontalStackView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kOmniboxContainerMinimumHeight],
    [_backButton.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    omniboxLeadingConstraint,
    [self.view.trailingAnchor
        constraintEqualToAnchor:_horizontalStackView.trailingAnchor
                       constant:kOmniboxContainerHorizontalPadding],
    [_webViewContainer.topAnchor
        constraintEqualToAnchor:_horizontalStackView.bottomAnchor
                       constant:kWebContainerTopPadding],
  ]];
  AddSameConstraintsToSides(
      self.webViewContainer, self.view,
      LayoutSides::kLeading | LayoutSides::kBottom | LayoutSides::kTrailing);
}

#pragma mark - LensResultPageConsumer

- (void)setWebView:(UIView*)webView {
  if (_webView == webView) {
    return;
  }

  if (_webView.superview == self.webViewContainer) {
    [_webView removeFromSuperview];
  }
  _webView = webView;

  _webView.translatesAutoresizingMaskIntoConstraints = NO;
  if (!_webView || !self.webViewContainer) {
    return;
  }

  [self.webViewContainer addSubview:_webView];
  AddSameConstraints(_webView, self.webViewContainer);
}

- (void)setBackgroundColor:(UIColor*)backgroundColor {
  self.view.backgroundColor = backgroundColor;
}

#pragma mark - Private

/// Handles back button taps.
- (void)didTapBackButton:(UIView*)button {
  // TODO(crbug.com/347239663): Handle back button tap.
}

/// Handles omnibox taps.
- (void)didTapOmniboxContainer:(UIView*)view {
  // TODO(crbug.com/347237539): Handle omnibox tap.
}

@end
