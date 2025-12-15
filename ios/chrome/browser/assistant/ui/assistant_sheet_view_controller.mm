// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller.h"

@implementation AssistantSheetViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor systemBackgroundColor];

  // Basic setup for now
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = @"WIP. Hi, how can I help?";
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:titleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [titleLabel.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [titleLabel.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
  ]];

  // TODO(crbug.com/469050167): Implement UI logic.
}

@end
