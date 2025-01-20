// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/ui/parent_access_bottom_sheet_view_controller.h"

#import "base/functional/callback.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation ParentAccessBottomSheetViewController {
  UIView* _webView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.topAlignedLayout = YES;
  self.scrollEnabled = NO;
  [super viewDidLoad];
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.detents =
      @[ [UISheetPresentationControllerDetent mediumDetent] ];
  presentationController.selectedDetentIdentifier =
      UISheetPresentationControllerDetentIdentifierMedium;
}

#pragma mark - ParentAccessConsumer

- (void)setWebView:(UIView*)view {
  if (_webView == view) {
    return;
  }
  _webView = view;
  [self.view addSubview:_webView];
  AddSameConstraints(_webView, self.view);
}

@end
