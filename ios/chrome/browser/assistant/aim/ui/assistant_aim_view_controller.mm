// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/aim/ui/assistant_aim_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

constexpr CGFloat kContentMargin = 16.0;
constexpr CGFloat kTitleVerticalMargin = 12.0;

}  // namespace

@implementation AssistantAIMViewController {
  UILabel* _titleLabel;
  UILabel* _label;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setUpHeader];

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

  [NSLayoutConstraint activateConstraints:@[
    [_label.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor
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

#pragma mark - Private

// Sets up the title.
- (void)setUpHeader {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.text = @"AI Assistant";
  _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_titleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                          constant:kTitleVerticalMargin],
    [_titleLabel.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
  ]];
}

@end
