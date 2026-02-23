// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_view_controller.h"

#import <QuartzCore/QuartzCore.h>

#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {
/// The padding for the close button.
const CGFloat kCloseButtonTopMargin = 7.0f;
const CGFloat kCloseButtonDefaultPadding = 10.0f;
/// The trailing and top padding for the input plate container.
const CGFloat kInputPlateTrailingPadding = 8.0f;
const CGFloat kInputPlateTopPadding = 4.0f;
/// The size for the close button.
const CGFloat kCloseButtonSize = 30.0f;
/// The ammount of padding to add vertically to the incognito view content.
const CGFloat kIncognitoVerticalPadding = 24.0f;
/// The bottom margin between the composebox and the container.
const CGFloat kBlurBottomMargin = 20.0f;
/// Margin between the composebox and the container when reduced transparency is
/// enabled.
const CGFloat kReducedTransparencyInputPlateBottomMargin = 4.0f;

/// The image for the close button.
UIImage* CloseButtonImage(UIColor* backgroundColor, BOOL highlighted) {
  NSArray<UIColor*>* palette =
      @[ [UIColor colorNamed:kTextfieldPlaceholderColor], backgroundColor ];

  if (highlighted) {
    palette = @[
      [[UIColor colorNamed:kTextfieldPlaceholderColor]
          colorWithAlphaComponent:0.3],
      [backgroundColor colorWithAlphaComponent:0.6]
    ];
  }

  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseButtonSize
                          weight:UIImageSymbolWeightLight
                           scale:UIImageSymbolScaleMedium];

  return SymbolWithPalette(DefaultSymbolWithConfiguration(
                               kXMarkCircleFillSymbol, symbolConfiguration),
                           palette);
}

}  // namespace

@interface ComposeboxViewController () <UIScrollViewDelegate>
@end

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

  // The view containing the incognito informations.
  IncognitoView* _incognitoView;

  // Whether the view is visible.
  BOOL _viewVisible;

  // Helpers for tracking the incognito scrolling position.
  BOOL _incognitoScrolledAtTop;
  BOOL _scrollIncognitoToTopOnNextLayout;

  // Last known preferred content height of the omnibox view controller.
  CGFloat _omniboxPreferredContentHeight;
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
  self.view.keyboardLayoutGuide.usesBottomSafeArea = NO;

  // Close button.
  UIColor* closeButtonBackgroundColor = _theme.closeButtonBackgroundColor;
  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];
  config.image = CloseButtonImage(closeButtonBackgroundColor, NO);
  config.contentInsets = NSDirectionalEdgeInsetsZero;
  _closeButton = [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  _closeButton.accessibilityIdentifier =
      kOmniboxCancelButtonAccessibilityIdentifier;
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

  // Omnibox popup container.
  _omniboxPopupContainer = [[UIView alloc] init];
  _omniboxPopupContainer.hidden = YES;
  _omniboxPopupContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view insertSubview:_omniboxPopupContainer atIndex:0];

  if (_theme.useIncognitoViewFallback) {
    _incognitoView = [[IncognitoView alloc] init];
    _incognitoView.translatesAutoresizingMaskIntoConstraints = NO;
    _incognitoView.delegate = self;
    _incognitoView.hidden = YES;

    [self.view insertSubview:_incognitoView atIndex:0];
  }

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(setupConstraints)
             name:UIAccessibilityReduceTransparencyStatusDidChangeNotification
           object:nil];

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(userInterfaceStyleChanged)];

  if (_theme.useIncognitoViewFallback) {
    [self
        registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                     withAction:@selector(preferredContentSizeCategoryChanged)];
  }

  if (IsComposeboxIpadEnabled()) {
    __weak ComposeboxViewController* weakSelf = self;
    [self registerForTraitChanges:@[ UITraitHorizontalSizeClass.class ]
                      withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                    UITraitCollection* previousCollection) {
                        [weakSelf setupConstraints];
                        if (traitEnvironment.traitCollection
                                .horizontalSizeClass !=
                            previousCollection.horizontalSizeClass) {
                          [weakSelf.delegate
                                  composeboxHorizontalSizeClassDidChange];
                        }
                      }];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  _viewVisible = YES;
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  _viewVisible = NO;
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  // If the view is visible and the incognito is scrolled to top, keep the
  // position when the content offset are readjusted.
  // (e.g. when laying out the subviews after a device orientation change).
  if (_viewVisible && _incognitoScrolledAtTop) {
    _scrollIncognitoToTopOnNextLayout = YES;
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [_inputViewController.view layoutIfNeeded];
  _fadeGradient.frame = _progressiveBlurEffect.bounds;

  [self computeScrollInsets];

  // If the view is not visible, scroll to top programatically to avoid a
  // visual overlap.
  if (!_viewVisible || _scrollIncognitoToTopOnNextLayout) {
    [_incognitoView
        setContentOffset:CGPointMake(0, -_incognitoView.contentInset.top)
                animated:NO];
    _scrollIncognitoToTopOnNextLayout = NO;
  }

  [_presenter setPreferredOmniboxPosition:_theme.isTopInputPlate
                                              ? ToolbarType::kPrimary
                                              : ToolbarType::kSecondary];
}

- (void)computeScrollInsets {
  // Intrinsic insets.
  UIEdgeInsets visualInsets = [_incognitoView intrinsicContentVisualInsets];
  CGRect inputPlateFrame = _inputViewController.view.frame;
  [_inputViewController.view layoutIfNeeded];
  [_incognitoView layoutIfNeeded];

  if ([self currentInputPlatePosition] == ComposeboxInputPlatePosition::kTop) {
    [_presenter
        setAdditionalVerticalContentInset:UIEdgeInsetsMake(
                                              _inputViewController.inputHeight,
                                              0, 0, 0)];

    CGFloat thresholdOffset = inputPlateFrame.size.height +
                              inputPlateFrame.origin.y -
                              self.view.safeAreaInsets.top;
    CGFloat requiredOffset =
        thresholdOffset - visualInsets.top + kIncognitoVerticalPadding;
    _incognitoView.contentInset = UIEdgeInsetsMake(requiredOffset, 0, 0, 0);
  } else if ([self currentInputPlatePosition] ==
             ComposeboxInputPlatePosition::kBottom) {
    // Add inset so the first suggestion is not covered by the close button.
    [_presenter
        setAdditionalVerticalContentInset:UIEdgeInsetsMake(
                                              kCloseButtonSize, 0,
                                              _inputViewController.inputHeight,
                                              0)];

    CGFloat paddingBottom = self.view.frame.size.height -
                            inputPlateFrame.size.height -
                            inputPlateFrame.origin.y;
    CGFloat thresholdOffset = inputPlateFrame.size.height + paddingBottom;
    CGFloat requiredOffset =
        thresholdOffset - visualInsets.bottom + kIncognitoVerticalPadding;
    _incognitoView.contentInset = UIEdgeInsetsMake(0, 0, requiredOffset + 0, 0);
  } else if ([self currentInputPlatePosition] ==
             ComposeboxInputPlatePosition::kiPad) {
    [_presenter
        setAdditionalVerticalContentInset:UIEdgeInsetsMake(
                                              _inputViewController.inputHeight,
                                              0, 0, 0)];
  }
}

- (void)userInterfaceStyleChanged {
  [self updateBlurVisibility];
}

- (void)preferredContentSizeCategoryChanged {
  [self.view layoutIfNeeded];
  [self computeScrollInsets];
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
  NSMutableArray* closeButtonConstraints = [@[
    [_closeButton.trailingAnchor
        constraintEqualToAnchor:safeAreaGuide.trailingAnchor
                       constant:-kCloseButtonDefaultPadding],
    [_closeButton.heightAnchor constraintEqualToConstant:kCloseButtonSize],
    [_closeButton.widthAnchor constraintEqualToAnchor:_closeButton.heightAnchor]
  ] mutableCopy];

  [_constraintsForCurrentPosition addObjectsFromArray:@[
    [_omniboxPopupContainer.leadingAnchor
        constraintEqualToAnchor:safeAreaGuide.leadingAnchor],
    [_omniboxPopupContainer.trailingAnchor
        constraintEqualToAnchor:safeAreaGuide.trailingAnchor],
    [_omniboxPopupContainer.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [_omniboxPopupContainer.topAnchor
        constraintEqualToAnchor:safeAreaGuide.topAnchor],
  ]];

  // Constraints for the incognito info view.
  if (_theme.useIncognitoViewFallback) {
    [_constraintsForCurrentPosition addObjectsFromArray:@[
      [_incognitoView.topAnchor
          constraintEqualToAnchor:safeAreaGuide.topAnchor],
      [_incognitoView.bottomAnchor
          constraintEqualToAnchor:self.view.bottomAnchor],
      // Intentionally exceeding the safe area. Internally the inconito view
      // resizes to be as big as the superview, regardless the superview.
      [_incognitoView.leadingAnchor
          constraintEqualToAnchor:self.view.leadingAnchor],
      [_incognitoView.trailingAnchor
          constraintEqualToAnchor:self.view.trailingAnchor]
    ]];
  }

  [_progressiveBlurEffect removeFromSuperview];
  [_closeButton removeFromSuperview];

  [self.view insertSubview:_closeButton belowSubview:_inputViewController.view];
  switch ([self currentInputPlatePosition]) {
    case ComposeboxInputPlatePosition::kBottom: {
      _progressiveBlurEffect = [self
          createBlurBackgroundEffectForPosition:[self
                                                    currentInputPlatePosition]];
      [self.view insertSubview:_progressiveBlurEffect
                  aboveSubview:_omniboxPopupContainer];
      AddSameConstraintsToSidesWithInsets(
          _progressiveBlurEffect, _inputViewController.view, LayoutSides::kTop,
          NSDirectionalEdgeInsetsMake(
              UIAccessibilityIsReduceTransparencyEnabled()
                  ? -kReducedTransparencyInputPlateBottomMargin
                  : -kBlurBottomMargin,
              0, 0, 0));
      AddSameConstraintsToSides(_progressiveBlurEffect, safeAreaGuide,
                                LayoutSides::kLeading | LayoutSides::kTrailing);

      NSLayoutConstraint* attachInputPlateToKeyboard =
          [_inputViewController.view.bottomAnchor
              constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                             constant:-kInputPlateMargin];
      attachInputPlateToKeyboard.priority = UILayoutPriorityDefaultHigh;

      [_constraintsForCurrentPosition addObjectsFromArray:@[
        [_progressiveBlurEffect.bottomAnchor
            constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor],
        [_closeButton.topAnchor
            constraintEqualToAnchor:safeAreaGuide.topAnchor
                           constant:kCloseButtonDefaultPadding],
        [_inputViewController.view.leadingAnchor
            constraintEqualToAnchor:safeAreaGuide.leadingAnchor
                           constant:kInputPlateMargin],
        [_inputViewController.view.trailingAnchor
            constraintEqualToAnchor:safeAreaGuide.trailingAnchor
                           constant:-kInputPlateMargin],
        attachInputPlateToKeyboard,
        [_inputViewController.view.bottomAnchor
            constraintLessThanOrEqualToAnchor:safeAreaGuide.bottomAnchor
                                     constant:-kInputPlateMargin],
        [_inputViewController.view.topAnchor
            constraintGreaterThanOrEqualToAnchor:_closeButton.bottomAnchor
                                        constant:kInputPlateMargin],
      ]];
      [_constraintsForCurrentPosition
          addObjectsFromArray:closeButtonConstraints];
      break;
    }
    case ComposeboxInputPlatePosition::kTop: {
      _progressiveBlurEffect = [self
          createBlurBackgroundEffectForPosition:[self
                                                    currentInputPlatePosition]];
      [self.view insertSubview:_progressiveBlurEffect
                  aboveSubview:_omniboxPopupContainer];
      AddSameConstraintsToSidesWithInsets(
          _progressiveBlurEffect, _inputViewController.view,
          LayoutSides::kBottom,
          NSDirectionalEdgeInsetsMake(
              0, 0,
              UIAccessibilityIsReduceTransparencyEnabled()
                  ? -kReducedTransparencyInputPlateBottomMargin
                  : -kBlurBottomMargin,
              0));
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

      CGFloat closeButtonTopMargin = AlignComposeboxCloseButtonToInputPlateTop()
                                         ? 0
                                         : kCloseButtonTopMargin;

      [_constraintsForCurrentPosition addObjectsFromArray:@[
        _constraintToCloseButton,
        constraintToMargin,
        [_closeButton.topAnchor
            constraintEqualToAnchor:_inputViewController.view.topAnchor
                           constant:closeButtonTopMargin],
        [_omniboxPopupContainer.leadingAnchor
            constraintEqualToAnchor:safeAreaGuide.leadingAnchor],
        [_omniboxPopupContainer.trailingAnchor
            constraintEqualToAnchor:safeAreaGuide.trailingAnchor],
        [_inputViewController.view.leadingAnchor
            constraintEqualToAnchor:safeAreaGuide.leadingAnchor
                           constant:kInputPlateMargin],
        [_inputViewController.view.topAnchor
            constraintEqualToAnchor:safeAreaGuide.topAnchor
                           constant:kInputPlateTopPadding],
        [_inputViewController.view.bottomAnchor
            constraintLessThanOrEqualToAnchor:self.view.keyboardLayoutGuide
                                                  .topAnchor
                                     constant:-kInputPlateMargin],
      ]];
      [_constraintsForCurrentPosition
          addObjectsFromArray:closeButtonConstraints];
      break;
    }
    case ComposeboxInputPlatePosition::kiPad: {
      _progressiveBlurEffect = [self
          createBlurBackgroundEffectForPosition:[self
                                                    currentInputPlatePosition]];
      [self.view insertSubview:_progressiveBlurEffect
                  aboveSubview:_omniboxPopupContainer];
      AddSameConstraintsToSidesWithInsets(
          _progressiveBlurEffect, _inputViewController.view,
          LayoutSides::kBottom,
          NSDirectionalEdgeInsetsMake(
              0, 0,
              UIAccessibilityIsReduceTransparencyEnabled()
                  ? -kReducedTransparencyInputPlateBottomMargin
                  : -kBlurBottomMargin,
              0));
      AddSameConstraintsToSides(
          _progressiveBlurEffect, safeAreaGuide,
          LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);

      [_constraintsForCurrentPosition addObjectsFromArray:@[
        [_inputViewController.view.leadingAnchor
            constraintEqualToAnchor:safeAreaGuide.leadingAnchor
                           constant:kInputPlateMargin],
        [_inputViewController.view.topAnchor
            constraintEqualToAnchor:safeAreaGuide.topAnchor
                           constant:kInputPlateMargin],
      ]];
      if (IsRegularXRegularSizeClass(self.traitCollection)) {
        // Constraints for when the close button is hidden.
        [closeButtonConstraints addObjectsFromArray:@[
          [_inputViewController.view.trailingAnchor
              constraintEqualToAnchor:safeAreaGuide.trailingAnchor
                             constant:-kInputPlateMargin]
        ]];
        _closeButton.hidden = YES;
      } else {
        // Constraints for when the close button is shown.
        [closeButtonConstraints addObjectsFromArray:@[
          [_inputViewController.view.trailingAnchor
              constraintEqualToAnchor:_closeButton.leadingAnchor
                             constant:-kInputPlateTrailingPadding],
          [_closeButton.centerYAnchor
              constraintEqualToAnchor:_inputViewController.view.centerYAnchor]
        ]];

        _closeButton.hidden = NO;
      }
      [_constraintsForCurrentPosition
          addObjectsFromArray:closeButtonConstraints];
      break;
    }
    case ComposeboxInputPlatePosition::kMissing:
      break;
  }

  [NSLayoutConstraint activateConstraints:_constraintsForCurrentPosition];
}

- (UIView*)fadeViewForPosition:(ComposeboxInputPlatePosition)positon {
  UIView* fadeView = [[UIView alloc] init];
  fadeView.userInteractionEnabled = NO;
  fadeView.translatesAutoresizingMaskIntoConstraints = NO;
  fadeView.backgroundColor = _theme.composeboxBackgroundColor;

  _fadeGradient = nil;
  // When reduced transparency is enabled, show a solid background color.
  if (UIAccessibilityIsReduceTransparencyEnabled()) {
    return fadeView;
  }

  // Otherwise add a gradient to the background color.
  CAGradientLayer* gradientLayer = [[CAGradientLayer alloc] init];
  gradientLayer.locations = @[ @(0.0), @(0.5), @(1.0) ];
  gradientLayer.colors = @[
    (id)[UIColor clearColor].CGColor,
    (id)[[UIColor whiteColor] colorWithAlphaComponent:0.7].CGColor,
    (id)[[UIColor whiteColor] colorWithAlphaComponent:1.0].CGColor,
  ];
  switch (positon) {
    case ComposeboxInputPlatePosition::kiPad:  // Fall through
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
  _fadeGradient = gradientLayer;
  return fadeView;
}

- (UIView*)createBlurBackgroundEffectForPosition:
    (ComposeboxInputPlatePosition)positon {
  UIView* containerView = [self fadeViewForPosition:positon];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  containerView.userInteractionEnabled = NO;

  _blurEffectView = nil;
  // When reduced transparency is enabled, container view will show a solid
  // background color.
  if (UIAccessibilityIsReduceTransparencyEnabled()) {
    return containerView;
  }

  // Otherwise add a blur effect.
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

- (void)setExpectsClipboardSuggestion:(BOOL)expectsClipboardSuggestion {
  // To avoid UI flickering during the loading of the Incognito view, this logic
  // implements a conditional delay. Since the Incognito NTP is limited to
  // clipboard-based suggestions, we proactively check the clipboard status.
  // The view remains hidden until the presence or absence of clipboard content
  // is verified, preventing a jumping UI state.
  _incognitoView.hidden = expectsClipboardSuggestion;
}

- (ComposeboxInputPlatePosition)currentInputPlatePosition {
  return _inputViewController.view ? _theme.inputPlatePosition
                                   : ComposeboxInputPlatePosition::kMissing;
}

#pragma mark - UIContentContainer

- (void)preferredContentSizeDidChangeForChildContentContainer:
    (id<UIContentContainer>)container {
  [super preferredContentSizeDidChangeForChildContentContainer:container];
  if (IsComposeboxIpadEnabled() &&
      UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad) {
    [self updatePreferredContentSize:container];
  }
}

- (void)updatePreferredContentSize:(id<UIContentContainer>)container {
  CGFloat containerHeight = container.preferredContentSize.height;
  CGFloat contentHeight = 0;
  if ([container isKindOfClass:[ComposeboxInputPlateViewController class]]) {
    // If omnibox has no results suggestions, then the input plate should be the
    // tallest content.
    CGFloat tallestHeight =
        _omniboxPopupContainer.hidden
            ? containerHeight + kBlurBottomMargin
            : containerHeight + _omniboxPreferredContentHeight;
    contentHeight = tallestHeight;
  } else {
    // Calculate content height knowing the content size of the omnibox popup
    // table view.
    _omniboxPreferredContentHeight = containerHeight;
    contentHeight = _inputViewController.inputHeight +
                    std::max(_omniboxPreferredContentHeight, kBlurBottomMargin);
  }
  CGFloat totalHeight = contentHeight + kInputPlateMargin;
  if (self.preferredContentSize.height != totalHeight) {
    self.preferredContentSize =
        CGSizeMake(self.view.bounds.size.width, totalHeight);
  }
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
    AddSameConstraintsWithInsets(webView, _omniboxPopupContainer,
                                 NSDirectionalEdgeInsetsMake(4, 0, 0, 0));
  }
}

#pragma mark - Action

- (void)closeButtonTapped {
  [self.delegate composeboxViewControllerDidTapCloseButton:self];
}

#pragma mark - OmniboxPopupPresenterDelegate

- (void)popupDidInitializePresenter:(OmniboxPopupPresenter*)presenter {
  _presenter = presenter;
}

- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter {
  return _omniboxPopupContainer;
}

- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter {
  return self;
}

- (UIColor*)popupBackgroundColorForPresenter:(OmniboxPopupPresenter*)presenter {
  return self.view.backgroundColor;
}

- (GuideName*)omniboxGuideNameForPresenter:(OmniboxPopupPresenter*)presenter {
  return nil;
}

- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter {
  _incognitoView.hidden = YES;
  _omniboxPopupContainer.hidden = NO;
  [self.proxiedPresenterDelegate popupDidOpenForPresenter:presenter];
}

- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter {
  _incognitoView.hidden = NO;
  _omniboxPopupContainer.hidden = YES;
  [self.proxiedPresenterDelegate popupDidCloseForPresenter:presenter];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [self.view endEditing:YES];
}

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  _incognitoScrolledAtTop =
      _incognitoView.contentOffset.y <= -_incognitoView.contentInset.top + 1;
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self.delegate composeboxViewControllerDidTapCloseButton:self];
  return YES;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  [self.delegate composeboxViewControllerDidTapCloseButton:self];
}

@end
