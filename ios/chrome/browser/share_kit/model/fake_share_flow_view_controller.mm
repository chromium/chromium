// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/fake_share_flow_view_controller.h"

#import "ios/chrome/browser/share_kit/model/test_constants.h"

@implementation FakeShareFlowViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kFakeShareFlowViewControllerIdentifier;

  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(handleCancelButton)];
}

- (void)handleCancelButton {
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
