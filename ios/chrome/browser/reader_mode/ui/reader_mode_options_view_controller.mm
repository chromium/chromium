// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation ReaderModeOptionsViewController

#pragma mark - Initialization

- (instancetype)init {
  self = [super init];
  if (self) {
    self.sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent mediumDetent] ];
    self.sheetPresentationController.largestUndimmedDetentIdentifier =
        [UISheetPresentationControllerDetent mediumDetent].identifier;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
}

@end
