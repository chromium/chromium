// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/host_fetching_error_view_controller.h"

#import <MaterialComponents/MDCTypography.h>
#import <MaterialComponents/MaterialButtons.h>

#include "remoting/base/string_resources.h"
#import "remoting/ios/app/remoting_theme.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kPadding = 20;
static const CGFloat kLineSpace = 30;

// The actual width will be min(kMaxWidth, screen width)
static const CGFloat kMaxWidth = 500;

// Adjust for the padding already existed inside the button.
static const CGFloat kButtonRightPaddingAdjustment = -10;
static const CGFloat kButtonBottomPaddingAdjustment = -5;

@implementation HostFetchingErrorViewController {
  UILabel* _label;
}

@synthesize onRetryCallback = _onRetryCallback;

- (instancetype)init {
  if ((self = [super init])) {
    // Label should be created right under init because it may be accessed
    // before the view is loaded.
    _label = [[UILabel alloc] initWithFrame:CGRectZero];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* contentView = [[UIView alloc] initWithFrame:CGRectZero];
  contentView.backgroundColor = RemotingTheme.setupListBackgroundColor;
  contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:contentView];

  _label.font = MDCTypography.body1Font;
  _label.numberOfLines = 0;
  _label.lineBreakMode = NSLineBreakByWordWrapping;
  _label.textColor = RemotingTheme.setupListTextColor;
  _label.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_label];

  MDCButton* button = [[MDCButton alloc] initWithFrame:CGRectZero];
  [button setTitle:l10n_util::GetNSString(IDS_RETRY)
          forState:UIControlStateNormal];
  [button setBackgroundColor:UIColor.clearColor forState:UIControlStateNormal];
  [button setTitleColor:RemotingTheme.flatButtonTextColor
               forState:UIControlStateNormal];
  [button sizeToFit];
  [button addTarget:self
                action:@selector(didTapRetry:)
      forControlEvents:UIControlEventTouchUpInside];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:button];

  NSLayoutConstraint* maxWidthConstraint =
      [contentView.widthAnchor constraintEqualToConstant:kMaxWidth];
  maxWidthConstraint.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint activateConstraints:@[
    maxWidthConstraint,

    // Trumps |maxWidthConstraint| when necessary.
    [contentView.widthAnchor
        constraintLessThanOrEqualToAnchor:self.view.widthAnchor],
    [contentView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [contentView.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],

    [_label.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                         constant:kPadding],
    [_label.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor
                                          constant:-kPadding],
    [_label.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                     constant:kPadding],

    [button.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kPadding - kButtonRightPaddingAdjustment],
    [button.topAnchor constraintEqualToAnchor:_label.bottomAnchor
                                     constant:kLineSpace],
    [button.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kPadding - kButtonBottomPaddingAdjustment],
  ]];
}

- (UILabel*)label {
  return _label;
}

#pragma mark - Private

- (void)didTapRetry:(id)button {
  if (_onRetryCallback) {
    _onRetryCallback();
  }
}

@end
