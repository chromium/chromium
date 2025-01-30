// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/ui/parent_access_bottom_sheet_view_controller.h"

#import "base/functional/callback.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/supervised_user/ui/constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/device_form_factor.h"

namespace {
// Custom detent identifier for when the bottom sheet is expanded.
NSString* const kCustomBottomSheetDetentIdentifier = @"customBottomSheetDetent";
}  // namespace

@implementation ParentAccessBottomSheetViewController {
  // WebView containing the parent access widget.
  UIView* _webView;
  // Whether the web view should be hidden.
  BOOL _webViewHidden;
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
  _webView.hidden = _webViewHidden;
  [self.view addSubview:_webView];
  AddSameConstraints(_webView, self.view);
}

- (void)setWebViewHidden:(BOOL)hidden {
  if (_webViewHidden == hidden) {
    return;
  }
  _webViewHidden = hidden;
  _webView.hidden = hidden;
}

#pragma mark - Private

// Returns a custom detent between the medium and large detents.
// This is necessary because the WebView height is dynamic and
// `preferredHeightDetent` is not applicable.
// TODO(crbug.com/384514294): Update the custom height if needed.
- (UISheetPresentationControllerDetent*)customHeightDetentWithIdentifier:
    (NSString*)identifier {
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    CGFloat largeDetentHeight = [UISheetPresentationControllerDetent.largeDetent
        resolvedValueInContext:context];
    CGFloat mediumDetentHeight =
        [UISheetPresentationControllerDetent.mediumDetent
            resolvedValueInContext:context];
    // Make sure detent is at least 75% of the maximum detent.
    return MAX(mediumDetentHeight, largeDetentHeight * 0.75);
  };

  return
      [UISheetPresentationControllerDetent customDetentWithIdentifier:identifier
                                                             resolver:resolver];
}

- (void)updateBottomSheetDetents {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  switch (ui::GetDeviceFormFactor()) {
    case ui::DeviceFormFactor::DEVICE_FORM_FACTOR_PHONE:
      if (IsPortrait(self.view.window)) {
        // In portrait mode, the bottom sheet occupies the bottom half of the
        // screen.
        presentationController.detents =
            @[ [UISheetPresentationControllerDetent mediumDetent] ];
        presentationController.selectedDetentIdentifier =
            UISheetPresentationControllerDetentIdentifierMedium;
      } else {
        // In landscape mode, the bottom sheet is centered horizontally and
        // occupies half of the screen width.
        presentationController.detents =
            @[ [UISheetPresentationControllerDetent largeDetent] ];
        presentationController.selectedDetentIdentifier =
            UISheetPresentationControllerDetentIdentifierLarge;
      }
      break;
    default:
      // On devices where the screen width difference between portrait and
      // landscape is less pronounced, display the bottom sheet with a custom
      // detent.
      presentationController.detents = @[
        [self
            customHeightDetentWithIdentifier:kCustomBottomSheetDetentIdentifier]
      ];
      presentationController.selectedDetentIdentifier =
          kCustomBottomSheetDetentIdentifier;
      break;
  }
}

@end
