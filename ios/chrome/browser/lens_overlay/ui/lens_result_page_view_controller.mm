// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

/// Minimum padding between the top of the view and the top of the web
/// container.
const CGFloat kWebContainerTopPadding = 10;

}  // namespace

@interface LensResultPageViewController ()

/// Web view in `_webViewContainer`.
@property(nonatomic, strong) UIView* webView;

@end

@implementation LensResultPageViewController {
  /// Container for the web view.
  UIView* _webViewContainer;
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

  CHECK(_webViewContainer, kLensOverlayNotFatalUntil);
  _webViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_webViewContainer];

  AddSameConstraintsToSides(
      _webViewContainer, self.view,
      LayoutSides::kLeading | LayoutSides::kBottom | LayoutSides::kTrailing);
  [NSLayoutConstraint activateConstraints:@[
    [_webViewContainer.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:kWebContainerTopPadding],
  ]];
}

#pragma mark - LensResultPageConsumer

- (void)setWebView:(UIView*)webView {
  if (_webView == webView) {
    return;
  }

  if (_webView.superview == _webViewContainer) {
    [_webView removeFromSuperview];
  }
  _webView = webView;

  _webView.translatesAutoresizingMaskIntoConstraints = NO;
  if (!_webView || !_webViewContainer) {
    return;
  }

  [_webViewContainer addSubview:_webView];
  AddSameConstraints(_webView, _webViewContainer);
}

@end
