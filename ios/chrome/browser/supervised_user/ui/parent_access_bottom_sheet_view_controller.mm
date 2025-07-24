// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/ui/parent_access_bottom_sheet_view_controller.h"

#import "base/functional/callback.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/supervised_user/ui/constants.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_bottom_sheet_view_controller_presentation_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

constexpr CGFloat kCloseButtonPadding = 16;
constexpr CGFloat kWidgetContentHeight = 410;

// Custom detent identifier for when the bottom sheet is expanded.
NSString* const kCustomBottomSheetDetentIdentifier = @"customBottomSheetDetent";

UIImage* CloseButtonImage(BOOL highlighted) {
  NSArray<UIColor*>* palette = @[
    [UIColor colorNamed:kGrey500Color],
    [UIColor colorNamed:kGrey100Color],
  ];

  if (highlighted) {
    NSMutableArray<UIColor*>* transparentPalette =
        [[NSMutableArray alloc] init];
    [palette enumerateObjectsUsingBlock:^(UIColor* color, NSUInteger idx,
                                          BOOL* stop) {
      [transparentPalette addObject:[color colorWithAlphaComponent:0.6]];
    }];
    palette = [transparentPalette copy];
  }

  return SymbolWithPalette(
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, 30), palette);
}

}  // namespace

@interface ParentAccessBottomSheetViewController ()

// WebView containing the parent access widget.
@property(nonatomic, strong) UIView* webView;

@end

@implementation ParentAccessBottomSheetViewController {
  // Whether the web view should be hidden.
  BOOL _webViewHidden;
  // The button to close the view.
  UIButton* _closeButton;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kParentAccessViewAccessibilityIdentifier;
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self updateBottomSheetDetents];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self updateBottomSheetDetents];
}

#pragma mark - ParentAccessConsumer

- (void)setWebView:(UIView*)view {
  if (_webView == view) {
    return;
  }
  _webView = view;
  _webView.translatesAutoresizingMaskIntoConstraints = NO;
  _webView.hidden = _webViewHidden;
  [self.view addSubview:_webView];
  AddSameConstraints(_webView, self.view);
  [self addCloseButton];
}

- (void)setWebViewHidden:(BOOL)hidden {
  if (_webViewHidden == hidden) {
    return;
  }
  _webViewHidden = hidden;

  if (hidden) {
    _webView.hidden = YES;
  } else {
    // Prevent a white flash in dark mode during WebView visibility changes by
    // animating the transition.
    __weak __typeof(self) weakSelf = self;
    [UIView transitionWithView:_webView
                      duration:0.3
                       options:UIViewAnimationOptionTransitionCrossDissolve
                    animations:^{
                      weakSelf.webView.hidden = NO;
                    }
                    completion:nil];
  }
}

#pragma mark - UIAccessibilityAction

// Dismiss the bottom sheet via the escape accessibility gesture.
- (BOOL)accessibilityPerformEscape {
  [self closeBottomSheetRequested];
  return YES;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  [self closeBottomSheetRequested];
}

#pragma mark - Private

// Returns a custom detent between the medium and large detents.
// This is necessary because the WebView height is dynamic and
// `preferredHeightDetent` is not applicable.
- (UISheetPresentationControllerDetent*)customHeightDetentWithIdentifier:
    (NSString*)identifier {
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    CGFloat largeDetentHeight =
        [[UISheetPresentationControllerDetent largeDetent]
            resolvedValueInContext:context];
    // Limit custom detent height at the maximum height.
    return MIN(kWidgetContentHeight, largeDetentHeight);
  };

  return
      [UISheetPresentationControllerDetent customDetentWithIdentifier:identifier
                                                             resolver:resolver];
}

- (void)updateBottomSheetDetents {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;

  presentationController.detents = @[ [self
      customHeightDetentWithIdentifier:kCustomBottomSheetDetentIdentifier] ];
  presentationController.selectedDetentIdentifier =
      kCustomBottomSheetDetentIdentifier;
}

- (void)closeBottomSheetRequested {
  // Hide the WebView to prevent a white flash in dark mode.
  [self setWebViewHidden:YES];
  [self.presentationDelegate closeBottomSheetRequested:self];
}

// Creates, initializes, and adds `_closeButton` to the bottom sheet.
- (void)addCloseButton {
  UIButtonConfiguration* closeButtonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  closeButtonConfiguration.contentInsets = NSDirectionalEdgeInsetsZero;
  closeButtonConfiguration.buttonSize = UIButtonConfigurationSizeSmall;
  closeButtonConfiguration.accessibilityLabel =
      l10n_util::GetNSString(IDS_CLOSE);

  __weak __typeof(self) weakSelf = self;
  _closeButton = [UIButton
      buttonWithConfiguration:closeButtonConfiguration
                primaryAction:[UIAction actionWithHandler:^(UIAction* action) {
                  [weakSelf closeBottomSheetRequested];
                }]];

  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  _closeButton.accessibilityIdentifier =
      kParentAccessCloseButtonAccessibilityIdentifier;
  _closeButton.pointerInteractionEnabled = YES;

  _closeButton.configurationUpdateHandler = ^(UIButton* button) {
    UIButtonConfiguration* updatedConfig = button.configuration;
    switch (button.state) {
      case UIControlStateHighlighted:
        updatedConfig.image = CloseButtonImage(/*highlighted=*/YES);
        break;
      case UIControlStateNormal:
        updatedConfig.image = CloseButtonImage(/*highlighted=*/NO);
        break;
    }
    button.configuration = updatedConfig;
  };

  [self.view addSubview:_closeButton];

  // Place the close button at the top right.
  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                           constant:kCloseButtonPadding],
    [_closeButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kCloseButtonPadding]
  ]];
}

@end
