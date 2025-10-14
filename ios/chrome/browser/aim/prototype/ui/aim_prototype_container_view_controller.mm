// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_container_view_controller.h"

#import "base/check_op.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_composebox_view_controller.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
/// The padding for the close button.
const CGFloat kCloseButtonPadding = 16.0f;
/// The horizontal and bottom padding for the input plate container.
const CGFloat kInputPlatePadding = 10.0f;
/// The size for the close button.
const CGFloat kCloseButtonSize = 30.0f;
/// The alpha for the close button.
const CGFloat kCloseButtonAlpha = 0.6f;
}  // namespace

@implementation AIMPrototypeContainerViewController {
  // Close button.
  UIButton* _closeButton;
  // Container for the input.
  UIView* _inputContainer;
  // Container for the omnibox popup.
  UIView* _omniboxPopupContainer;
  // WebView for the SRP, when AI Mode Immersive SRP is enabled.
  UIView* _webView;

  // The presenter for the omnibox popup.
  __weak OmniboxPopupPresenter* _presenter;
  // View controller for the AIM prototype composebox.
  __weak AIMPrototypeComposeboxViewController* _inputView;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  UILayoutGuide* safeAreaGuide = self.view.safeAreaLayoutGuide;

  // Close button.
  _closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseButtonSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  UIImage* buttonImage =
      SymbolWithPalette(DefaultSymbolWithConfiguration(kXMarkCircleFillSymbol,
                                                       symbolConfiguration),
                        @[
                          [[UIColor tertiaryLabelColor]
                              colorWithAlphaComponent:kCloseButtonAlpha],
                          [UIColor tertiarySystemFillColor]
                        ]);
  [_closeButton setImage:buttonImage forState:UIControlStateNormal];

  [_closeButton addTarget:self
                   action:@selector(closeButtonTapped)
         forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_closeButton];

  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.topAnchor constraintEqualToAnchor:safeAreaGuide.topAnchor
                                           constant:kCloseButtonPadding],
    [_closeButton.trailingAnchor
        constraintEqualToAnchor:safeAreaGuide.trailingAnchor
                       constant:-kCloseButtonPadding],
    [_closeButton.heightAnchor constraintEqualToConstant:kCloseButtonSize],
    [_closeButton.widthAnchor
        constraintEqualToAnchor:_closeButton.heightAnchor],
  ]];

  // Omnibox popup container.
  _omniboxPopupContainer = [[UIView alloc] init];
  _omniboxPopupContainer.hidden = YES;
  _omniboxPopupContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_omniboxPopupContainer];

  [NSLayoutConstraint activateConstraints:@[
    [_omniboxPopupContainer.topAnchor
        constraintEqualToAnchor:_closeButton.bottomAnchor],
    [_omniboxPopupContainer.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_omniboxPopupContainer.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_omniboxPopupContainer.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  // Input container.
  _inputContainer = [[UIView alloc] init];
  _inputContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_inputContainer];

  [NSLayoutConstraint activateConstraints:@[
    [_inputContainer.leadingAnchor
        constraintEqualToAnchor:safeAreaGuide.leadingAnchor
                       constant:kInputPlatePadding],
    [_inputContainer.trailingAnchor
        constraintEqualToAnchor:safeAreaGuide.trailingAnchor
                       constant:-kInputPlatePadding],
    [_inputContainer.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                       constant:-kInputPlatePadding],
  ]];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [_presenter setKeyboardAttachedBottomOmniboxHeight:_inputView.inputHeight];
}

- (void)addInputViewController:
    (AIMPrototypeComposeboxViewController*)inputViewController {
  [self loadViewIfNeeded];

  // Make sure we didn't already add an input.
  CHECK_EQ(_inputContainer.subviews.count, 0UL);

  [self addChildViewController:inputViewController];
  [_inputContainer addSubview:inputViewController.view];
  inputViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(_inputContainer, inputViewController.view);
  [inputViewController didMoveToParentViewController:self];
  _inputView = inputViewController;
}

#pragma mark - AIMPrototypeNavigationConsumer

- (void)setWebView:(UIView*)webView {
  if (_webView == webView) {
    return;
  }
  [_webView removeFromSuperview];
  _webView = webView;
  if (webView) {
    webView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view insertSubview:webView atIndex:0];
    AddSameConstraintsToSides(webView, self.view.safeAreaLayoutGuide,
                              LayoutSides::kLeading | LayoutSides::kTrailing);
    [NSLayoutConstraint activateConstraints:@[
      [webView.topAnchor constraintEqualToAnchor:_closeButton.bottomAnchor],
      [webView.bottomAnchor constraintEqualToAnchor:_inputContainer.topAnchor],
    ]];
  }
}

#pragma mark - Action

- (void)closeButtonTapped {
  [self.delegate aimPrototypeContainerViewControllerDidTapCloseButton:self];
}

#pragma mark - OmniboxPopupPresenterDelegate

- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter {
  return _omniboxPopupContainer;
}

- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter {
  return self;
}

- (UIColor*)popupBackgroundColorForPresenter:(OmniboxPopupPresenter*)presenter {
  _presenter = presenter;
  return [UIColor colorNamed:kPrimaryBackgroundColor];
}

- (GuideName*)omniboxGuideNameForPresenter:(OmniboxPopupPresenter*)presenter {
  return nil;
}

- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter {
  _omniboxPopupContainer.hidden = NO;
}

- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter {
  _omniboxPopupContainer.hidden = YES;
}

@end
