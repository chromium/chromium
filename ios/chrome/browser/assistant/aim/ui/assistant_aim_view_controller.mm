// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/aim/ui/assistant_aim_view_controller.h"

@implementation AssistantAIMViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // TODO(crbug.com/469050167): Clean this once consumer driven UI is
  // implemented (mediator will push the UI via consumer).
  UILabel* label = [[UILabel alloc] init];
  label.text = @"Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
               @"sed do eiusmod tempor incididunt ut labore et dolore "
               @"magna aliqua. Ut enim ad minim veniam, quis nostrud "
               @"exercitation ullamco laboris nisi ut aliquip ex ea "
               @"commodo consequat. Duis aute irure dolor in reprehenderit "
               @"in voluptate velit esse cillum dolore eu fugiat nulla "
               @"pariatur. Excepteur sint occaecat cupidatat non proident, "
               @"sunt in culpa qui officia deserunt mollit anim id est "
               @"laborum.";
  label.numberOfLines = 0;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:label];

  [NSLayoutConstraint activateConstraints:@[
    [label.topAnchor constraintEqualToAnchor:self.view.topAnchor constant:16],
    [label.bottomAnchor constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                                 constant:-16],
    [label.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                        constant:16],
    [label.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                         constant:-16],
  ]];
}

@end
