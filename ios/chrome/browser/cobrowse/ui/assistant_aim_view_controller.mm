// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

constexpr CGFloat kTitleVerticalMargin = 12.0;

}  // namespace

@implementation AssistantAIMViewController {
  UILabel* _titleLabel;
  UIView* _webStateView;
  NSArray<NSLayoutConstraint*>* _webStateViewConstraints;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setUpHeader];
  [self setUpWebStateView];
}

#pragma mark - AssistantAIMConsumer

- (void)setWebStateView:(UIView*)webStateView {
  if (_webStateView == webStateView) {
    return;
  }

  [_webStateView removeFromSuperview];
  _webStateView = webStateView;

  [self setUpWebStateView];
}

#pragma mark - Private

// Sets up the web state view.
- (void)setUpWebStateView {
  if (!_webStateView || !self.isViewLoaded) {
    return;
  }

  if (_webStateViewConstraints) {
    [NSLayoutConstraint deactivateConstraints:_webStateViewConstraints];
    _webStateViewConstraints = nil;
  }

  _webStateView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_webStateView];

  _webStateViewConstraints = @[
    [_webStateView.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor
                                            constant:kTitleVerticalMargin],
    [_webStateView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_webStateView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_webStateView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ];
  [NSLayoutConstraint activateConstraints:_webStateViewConstraints];
}

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
  ]];
  AddSameCenterXConstraint(_titleLabel, self.view);
}

@end
