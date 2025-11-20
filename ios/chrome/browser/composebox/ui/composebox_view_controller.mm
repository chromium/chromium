// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_view_controller.h"

#import "base/check_op.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
/// The padding for the close button.
const CGFloat kCloseButtonDefaultPadding = 16.0f;
const CGFloat kCloseButtonTopAlignedPadding = 22.0f;
/// The horizontal and bottom padding for the input plate container.
const CGFloat kInputPlatePadding = 10.0f;
/// The size for the close button.
const CGFloat kCloseButtonSize = 30.0f;
/// The alpha for the close button.
const CGFloat kCloseButtonAlpha = 0.6f;
}  // namespace

@implementation ComposeboxViewController {
  // Close button.
  UIButton* _closeButton;
  // Container for the omnibox popup.
  UIView* _omniboxPopupContainer;
  // WebView for the SRP, when AI Mode Immersive SRP is enabled.
  UIView* _webView;
  // The list of constraints for the current position. Updates once the current
  // position changes (e.g. on orientation change).
  NSMutableArray<NSLayoutConstraint*>* _constraintsForCurrentPosition;
  // The presenter for the omnibox popup.
  __weak OmniboxPopupPresenter* _presenter;
  // View controller for the composebox composebox.
  __weak ComposeboxInputPlateViewController* _inputViewController;
  // The theme of the composebox.
  ComposeboxTheme* _theme;
}

- (instancetype)initWithTheme:(ComposeboxTheme*)theme {
  self = [super init];
  if (self) {
    _theme = theme;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = _theme.composeboxBackgroundColor;

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

  // Omnibox popup container.
  _omniboxPopupContainer = [[UIView alloc] init];
  _omniboxPopupContainer.hidden = YES;
  _omniboxPopupContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_omniboxPopupContainer];

  [self setupConstraints];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [_presenter
      setKeyboardAttachedBottomOmniboxHeight:_inputViewController.inputHeight];
  [_presenter setPreferredOmniboxPosition:_theme.isTopInputPlate
                                              ? ToolbarType::kPrimary
                                              : ToolbarType::kSecondary];
}

- (void)addInputViewController:
    (ComposeboxInputPlateViewController*)inputViewController {
  [self loadViewIfNeeded];

  [self addChildViewController:inputViewController];
  [self.view addSubview:inputViewController.view];
  inputViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [inputViewController didMoveToParentViewController:self];
  _inputViewController = inputViewController;

  [_inputViewController.view
      setContentHuggingPriority:UILayoutPriorityRequired
                        forAxis:UILayoutConstraintAxisVertical];
  // Allow compression on the input view to limit it's height in the available
  // space (between the keyboard and the top of the view).
  [_inputViewController.view
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];

  [self setupConstraints];
}

- (void)setupConstraints {
  for (NSLayoutConstraint* staleConstraint in _constraintsForCurrentPosition) {
    staleConstraint.active = NO;
  }

  _constraintsForCurrentPosition = [[NSMutableArray alloc] init];

  UILayoutGuide* safeAreaGuide = self.view.safeAreaLayoutGuide;

  // Close button.
  [_constraintsForCurrentPosition addObjectsFromArray:@[
    [_closeButton.trailingAnchor
        constraintEqualToAnchor:safeAreaGuide.trailingAnchor
                       constant:-kCloseButtonDefaultPadding],
    [_closeButton.heightAnchor constraintEqualToConstant:kCloseButtonSize],
    [_closeButton.widthAnchor
        constraintEqualToAnchor:_closeButton.heightAnchor],
  ]];

  [_constraintsForCurrentPosition addObjectsFromArray:@[
    [_omniboxPopupContainer.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_omniboxPopupContainer.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  switch ([self currentInputPlatePosition]) {
    case ComposeboxInputPlatePosition::kBottom:
      [_constraintsForCurrentPosition addObjectsFromArray:@[
        [_closeButton.topAnchor
            constraintEqualToAnchor:safeAreaGuide.topAnchor
                           constant:kCloseButtonDefaultPadding],
        [_omniboxPopupContainer.topAnchor
            constraintEqualToAnchor:_closeButton.bottomAnchor],
        [_omniboxPopupContainer.bottomAnchor
            constraintEqualToAnchor:self.view.bottomAnchor],
        [_inputViewController.view.leadingAnchor
            constraintEqualToAnchor:safeAreaGuide.leadingAnchor
                           constant:kInputPlatePadding],
        [_inputViewController.view.trailingAnchor
            constraintEqualToAnchor:safeAreaGuide.trailingAnchor
                           constant:-kInputPlatePadding],
        [_inputViewController.view.bottomAnchor
            constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                           constant:-kInputPlatePadding],
        [_inputViewController.view.topAnchor
            constraintGreaterThanOrEqualToAnchor:_closeButton.bottomAnchor
                                        constant:kInputPlatePadding],
      ]];
      break;
    case ComposeboxInputPlatePosition::kTop:
      [_constraintsForCurrentPosition addObjectsFromArray:@[
        [_closeButton.topAnchor
            constraintEqualToAnchor:safeAreaGuide.topAnchor
                           constant:kCloseButtonTopAlignedPadding],
        [_omniboxPopupContainer.topAnchor
            constraintEqualToAnchor:_inputViewController.view.bottomAnchor],
        [_omniboxPopupContainer.leadingAnchor
            constraintEqualToAnchor:self.view.leadingAnchor],
        [_omniboxPopupContainer.trailingAnchor
            constraintEqualToAnchor:self.view.trailingAnchor],
        [_omniboxPopupContainer.bottomAnchor
            constraintEqualToAnchor:self.view.bottomAnchor],
        [_inputViewController.view.leadingAnchor
            constraintEqualToAnchor:safeAreaGuide.leadingAnchor
                           constant:kInputPlatePadding],
        [_inputViewController.view.trailingAnchor
            constraintEqualToAnchor:_closeButton.leadingAnchor
                           constant:-kInputPlatePadding],
        [_inputViewController.view.topAnchor
            constraintEqualToAnchor:safeAreaGuide.topAnchor
                           constant:kInputPlatePadding],
        [_inputViewController.view.bottomAnchor
            constraintLessThanOrEqualToAnchor:self.view.keyboardLayoutGuide
                                                  .topAnchor
                                     constant:-kInputPlatePadding],
      ]];
      break;
    case ComposeboxInputPlatePosition::kMissing:
      break;
  }

  [NSLayoutConstraint activateConstraints:_constraintsForCurrentPosition];
}

- (ComposeboxInputPlatePosition)currentInputPlatePosition {
  return _inputViewController.view ? _theme.inputPlatePosition
                                   : ComposeboxInputPlatePosition::kMissing;
}

#pragma mark - ComposeboxNavigationConsumer

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
      [webView.bottomAnchor
          constraintEqualToAnchor:_inputViewController.view.topAnchor],
    ]];
  }
}

#pragma mark - Action

- (void)closeButtonTapped {
  [self.delegate composeboxViewControllerDidTapCloseButton:self];
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
  return self.view.backgroundColor;
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
