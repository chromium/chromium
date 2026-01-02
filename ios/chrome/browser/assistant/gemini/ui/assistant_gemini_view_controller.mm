// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/gemini/ui/assistant_gemini_view_controller.h"

@implementation AssistantGeminiViewController {
  UILabel* _label;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // TODO(crbug.com/469050167): Replace with real UI.
  _label = [[UILabel alloc] init];
  _label.text = @"Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                @"sed do eiusmod tempor incididunt ut labore et dolore "
                @"magna aliqua. Ut enim ad minim veniam, quis nostrud "
                @"exercitation ullamco laboris nisi ut aliquip ex ea "
                @"commodo consequat. Duis aute irure dolor in reprehenderit "
                @"in voluptate velit esse cillum dolore eu fugiat nulla "
                @"pariatur. Excepteur sint occaecat cupidatat non proident, "
                @"sunt in culpa qui officia deserunt mollit anim id est "
                @"laborum.";
  _label.numberOfLines = 0;
  _label.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_label];

  const CGFloat kContentMargin = 16.0;
  [NSLayoutConstraint activateConstraints:@[
    [_label.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                     constant:kContentMargin],
    [_label.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                 constant:-kContentMargin],
    [_label.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                         constant:kContentMargin],
    [_label.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                          constant:-kContentMargin],
  ]];
}

#pragma mark - AssistantGeminiConsumer

- (void)setContentText:(NSString*)text {
  _label.text = text;
}

@end
