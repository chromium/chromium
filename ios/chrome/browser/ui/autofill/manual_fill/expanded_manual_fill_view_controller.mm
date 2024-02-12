// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/expanded_manual_fill_view_controller.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation ExpandedManualFillViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.accessibilityIdentifier = manual_fill::kExpandedManualFillViewID;
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

@end
