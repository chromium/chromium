// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/fake_share_kit_flow_view_controller.h"

#import "ios/chrome/browser/share_kit/model/test_constants.h"

@implementation FakeShareKitFlowViewController {
  FakeShareKitFlowType _type;
}

- (instancetype)initWithType:(FakeShareKitFlowType)type {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _type = type;
  }
  return self;
}

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

  UIView* view = self.view;

  UILabel* typeLabel = [[UILabel alloc] init];
  typeLabel.translatesAutoresizingMaskIntoConstraints = NO;
  typeLabel.textColor = UIColor.blackColor;
  [view addSubview:typeLabel];
  [NSLayoutConstraint activateConstraints:@[
    [typeLabel.centerYAnchor constraintEqualToAnchor:view.centerYAnchor],
    [typeLabel.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],
  ]];

  switch (_type) {
    case FakeShareKitFlowType::kShare:
      typeLabel.text = @"Share";
      view.accessibilityIdentifier = kFakeShareFlowIdentifier;
      break;
    case FakeShareKitFlowType::kManage:
      typeLabel.text = @"Manage";
      view.accessibilityIdentifier = kFakeManageFlowIdentifier;
      break;
    case FakeShareKitFlowType::kJoin:
      typeLabel.text = @"Join";
      view.accessibilityIdentifier = kFakeJoinFlowIdentifier;
      break;
  }
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
