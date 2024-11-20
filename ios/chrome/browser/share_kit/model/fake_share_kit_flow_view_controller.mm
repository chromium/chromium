// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/fake_share_kit_flow_view_controller.h"

@implementation FakeShareKitFlowViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.whiteColor;

  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(handleCancelButton)];

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemSave
                           target:self
                           action:@selector(handleSaveButton)];
}

- (void)handleCancelButton {
  if (_completionBlock) {
    _completionBlock(NO);
  }
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)handleSaveButton {
  if (_sharedGroupCompletionBlock) {
    _sharedGroupCompletionBlock([[NSUUID UUID] UUIDString]);
  }
  if (_completionBlock) {
    _completionBlock(YES);
  }
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
