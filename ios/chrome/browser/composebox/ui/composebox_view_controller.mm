// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_view_controller.h"

#import <QuartzCore/QuartzCore.h>

#import "base/check_op.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
/// The padding for the close button.
const CGFloat kCloseButtonTopMargin = 3.0f;
const CGFloat kCloseButtonDefaultPadding = 10.0f;
/// The horizontal and bottom padding for the input plate container.
const CGFloat kInputPlatePadding = 10.0f;
const CGFloat kInputPlateTrailingPadding = 8.0f;
const CGFloat kInputPlateTopPadding = 4.0f;
/// The size for the close button.
const CGFloat kCloseButtonSize = 30.0f;
/// The alpha for the close button.
const CGFloat kCloseButtonAlpha = 0.6f;
/// The image for the close button.
UIImage* CloseButtonImage(UIColor* backgroundColor, BOOL highlighted) {
  NSArray<UIColor*>* palette = @[
    [UIColor.tertiaryLabelColor colorWithAlphaComponent:kCloseButtonAlpha],
    backgroundColor
  ];

  if (highlighted) {
    palette = @[
      [UIColor.tertiaryLabelColor colorWithAlphaComponent:0.3],
      [backgroundColor colorWithAlphaComponent:0.6]
    ];
  }

  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseButtonSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];

  return SymbolWithPalette(DefaultSymbolWithConfiguration(
                               kXMarkCircleFillSymbol, symbolConfiguration),
                           palette);
}

}  // namespace

@implementation ComposeboxViewController {
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

  // The trailing input plate constraint to the close button.
  NSLayoutConstraint* _constraintToCloseButton;

  // The views respon sible for the fade effect on scroll.
  UIView* _progressiveBlurEffect;
  UIView* _blurEffectView;
  CALayer* _fadeGradient;
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
  UIColor* closeButtonBackgroundColor = _theme.closeButtonBackgroundColor;
  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];
  config.image = CloseButtonImage(closeButtonBackgroundColor, NO);
  config.contentInsets = NSDirectionalEdgeInsetsZero;
  _closeButton = [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  _closeButton.configuration = config;
  _closeButton.configurationUpdateHandler = ^(UIButton* button) {
    UIButtonConfiguration* updatedConfig = button.configuration;
    BOOL isHighlighted = button.state == UIControlStateHighlighted;
    updatedConfig.image =
        CloseButtonImage(closeButtonBackgroundColor, isHighlighted);
    button.configuration = updatedConfig;
    CGFloat scale = isHighlighted ? 0.95 : 1.0;
    [UIView animateWithDuration:0.1
                     animations:^{
                       button.transform =
                           CGAffineTransformMakeScale(scale, scale);
                     }];
  };

  [_closeButton addTarget:self
                   action:@selector(closeButtonTapped)
         forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_closeButton];

  // Omnibox popup container.
  _omniboxPopupContainer = [[UIView alloc] init];
  _omniboxPopupContainer.hidden = YES;
  _omniboxPopupContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view insertSubview:_omniboxPopupContainer atIndex:0];

  [self setupConstraints];

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(userInterfaceStyleChanged)];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [_inputViewController.view layoutIfNeeded];
  _fadeGradient.frame = _progressiveBlurEffect.bounds;
  [_presenter
      setKeyboardAttachedBottomOmniboxHeight:_inputViewController.inputHeight];

  if ([self currentInputPlatePosition] == ComposeboxInputPlatePosition::kTop) {
    [_presenter
        setAdditionalVerticalContentInset:_inputViewController.inputHeight];
  }

  [_presenter setPreferredOmniboxPosition:_theme.isTopInputPlate
                                              ? ToolbarType::kPrimary
                                              : ToolbarType::kSecondary];
}

- (void)userInterfaceStyleChanged {
  [self updateBlurVisibility];
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
    [_omniboxPopupContainer.bottomAnchor
        constraintEqualToAnchor:safeAreaGuide.bottomAnchor],
  ]];

  [_progressiveBlurEffect removeFromSuperview];

  switch ([self currentInputPlatePosition]) {
    case ComposeboxInputPlatePosition::kBottom:
      _progressiveBlurEffect = [self
          createBlurBackgroundEffectForPosition:[self
                                                    currentInputPlatePosition]];
      [self.view insertSubview:_progressiveBlurEffect
                  aboveSubview:_omniboxPopupContainer];
      AddSameConstraintsToSidesWithInsets(
          _progressiveBlurEffect, _inputViewController.view, LayoutSides::kTop,
          NSDirectionalEdgeInsetsMake(-20, 0, 0, 0));
      AddSameConstraintsToSides(_progressiveBlurEffect, safeAreaGuide,
                                LayoutSides::kBottom | LayoutSides::kLeading |
                                    LayoutSides::kTrailing);

      [_constraintsForCurrentPosition addObjectsFromArray:@[
        [_closeButton.topAnchor
            constraintEqualToAnchor:safeAreaGuide.topAnchor
                           constant:kCloseButtonDefaultPadding],
        [_omniboxPopupContainer.topAnchor
            constraintEqualToAnchor:_closeButton.bottomAnchor],
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
    case ComposeboxInputPlatePosition::kTop: {
      _progressiveBlurEffect = [self
          createBlurBackgroundEffectForPosition:[self
                                                    currentInputPlatePosition]];
      [self.view insertSubview:_progressiveBlurEffect
                  aboveSubview:_omniboxPopupContainer];
      AddSameConstraintsToSidesWithInsets(
          _progressiveBlurEffect, _inputViewController.view,
          LayoutSides::kBottom, NSDirectionalEdgeInsetsMake(0, 0, -20, 0));
      AddSameConstraintsToSides(
          _progressiveBlurEffect, safeAreaGuide,
          LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);

      _constraintToCloseButton = [_inputViewController.view.trailingAnchor
          constraintEqualToAnchor:_closeButton.leadingAnchor
                         constant:-kInputPlateTrailingPadding];
      auto constraintToMargin = [_inputViewController.view.trailingAnchor
          constraintEqualToAnchor:_closeButton.trailingAnchor
                         constant:0];
      constraintToMargin.priority = UILayoutPriorityRequired - 1;
      [_constraintsForCurrentPosition addObjectsFromArray:@[
        _constraintToCloseButton,
        constraintToMargin,
        [_closeButton.topAnchor
            constraintEqualToAnchor:_inputViewController.view.topAnchor
                           constant:kCloseButtonTopMargin],
        [_omniboxPopupContainer.topAnchor
            constraintEqualToAnchor:safeAreaGuide.topAnchor],
        [_omniboxPopupContainer.leadingAnchor
            constraintEqualToAnchor:safeAreaGuide.leadingAnchor],
        [_omniboxPopupContainer.trailingAnchor
            constraintEqualToAnchor:safeAreaGuide.trailingAnchor],
        [_inputViewController.view.leadingAnchor
            constraintEqualToAnchor:safeAreaGuide.leadingAnchor
                           constant:kInputPlatePadding],
        [_inputViewController.view.topAnchor
            constraintEqualToAnchor:safeAreaGuide.topAnchor
                           constant:kInputPlateTopPadding],
        [_inputViewController.view.bottomAnchor
            constraintLessThanOrEqualToAnchor:self.view.keyboardLayoutGuide
                                                  .topAnchor
                                     constant:-kInputPlatePadding],
      ]];
      break;
    }
    case ComposeboxInputPlatePosition::kMissing:
      break;
  }

  [NSLayoutConstraint activateConstraints:_constraintsForCurrentPosition];
}

- (UIView*)fadeViewForPosition:(ComposeboxInputPlatePosition)positon {
  UIView* fadeView = [[UIView alloc] init];
  fadeView.backgroundColor = _theme.composeboxBackgroundColor;

  CAGradientLayer* gradientLayer = [[CAGradientLayer alloc] init];
  gradientLayer.locations = @[ @(0.0), @(0.5), @(1.0) ];
  gradientLayer.colors = @[
    (id)[UIColor clearColor].CGColor,
    (id)[[UIColor whiteColor] colorWithAlphaComponent:0.7].CGColor,
    (id)[[UIColor whiteColor] colorWithAlphaComponent:1.0].CGColor,
  ];
  switch (positon) {
    case ComposeboxInputPlatePosition::kTop:
      gradientLayer.startPoint = CGPointMake(0.5, 1.0);
      gradientLayer.endPoint = CGPointMake(0.5, 0.4);
      break;
    case ComposeboxInputPlatePosition::kBottom:
      gradientLayer.startPoint = CGPointMake(0.5, 0.0);
      gradientLayer.endPoint = CGPointMake(0.5, 0.6);
      break;
    default:
      break;
  }

  fadeView.layer.mask = gradientLayer;
  fadeView.userInteractionEnabled = NO;
  fadeView.translatesAutoresizingMaskIntoConstraints = NO;

  _fadeGradient = gradientLayer;
  return fadeView;
}

- (UIView*)createBlurBackgroundEffectForPosition:
    (ComposeboxInputPlatePosition)positon {
  UIView* containerView = [self fadeViewForPosition:positon];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  containerView.userInteractionEnabled = NO;

  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleRegular];
  UIVisualEffectView* blurEffectView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];

  UIVibrancyEffect* vibrancy =
      [UIVibrancyEffect effectForBlurEffect:blurEffect
                                      style:UIVibrancyEffectStyleFill];
  UIVisualEffectView* vibrancyView =
      [[UIVisualEffectView alloc] initWithEffect:vibrancy];
  vibrancyView.translatesAutoresizingMaskIntoConstraints = NO;
  [blurEffectView.contentView addSubview:vibrancyView];
  AddSameConstraints(vibrancyView, blurEffectView.contentView);

  blurEffectView.translatesAutoresizingMaskIntoConstraints = NO;
  [containerView addSubview:blurEffectView];
  AddSameConstraints(blurEffectView, containerView);

  _blurEffectView = blurEffectView;
  [self updateBlurVisibility];

  return containerView;
}

- (void)updateBlurVisibility {
  BOOL darkStyle =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark;
  _blurEffectView.hidden = darkStyle || _theme.incognito;
}

- (void)expandInputPlateForDismissal {
  _constraintToCloseButton.active = NO;
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
