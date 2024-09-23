// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/host_setup_view_cell.h"

#import <MaterialComponents/MaterialTypography.h>

#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/view_utils.h"

static const CGFloat kNumberIconPadding = 16.f;
static const CGFloat kNumberIconSize = 45.f;
static const CGFloat kCellXPadding = 22.f;
static const CGFloat kCellYPadding = 28.f;

@interface HostSetupViewCell () {
  UIView* _numberContainerView;
  UILabel* _numberLabel;
  UILabel* _contentLabel;
}
@end

@implementation HostSetupViewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  if ((self = [super initWithStyle:style reuseIdentifier:reuseIdentifier])) {
    [self commonInit];
  }
  return self;
}

- (void)commonInit {
  self.isAccessibilityElement = YES;
  self.backgroundColor = RemotingTheme.setupListBackgroundColor;

  _numberContainerView = [[UIView alloc] init];
  _numberLabel = [[UILabel alloc] init];
  _contentLabel = [[UILabel alloc] init];

  _numberContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _numberLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _contentLabel.translatesAutoresizingMaskIntoConstraints = NO;

  _contentLabel.lineBreakMode = NSLineBreakByWordWrapping;
  _contentLabel.numberOfLines = 0;

  _numberContainerView.backgroundColor = RemotingTheme.hostOnlineColor;
  _numberLabel.textColor = RemotingTheme.setupListNumberColor;
  _contentLabel.textColor = RemotingTheme.setupListTextColor;
  _numberLabel.font = MDCTypography.titleFont;
  _contentLabel.font = MDCTypography.subheadFont;
  _numberContainerView.layer.cornerRadius = kNumberIconSize / 2.f;

  [self.contentView addSubview:_numberContainerView];
  [self.contentView addSubview:_contentLabel];
  [_numberContainerView addSubview:_numberLabel];

  UILayoutGuide* safeAreaLayoutGuide =
      remoting::SafeAreaLayoutGuideForView(self.contentView);

  NSArray* constraints = @[
    [_numberContainerView.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:kCellXPadding],
    [_numberContainerView.centerYAnchor
        constraintEqualToAnchor:_contentLabel.centerYAnchor],
    [_numberContainerView.widthAnchor
        constraintEqualToConstant:kNumberIconSize],
    [_numberContainerView.heightAnchor
        constraintEqualToConstant:kNumberIconSize],

    [_numberLabel.centerXAnchor
        constraintEqualToAnchor:_numberContainerView.centerXAnchor],
    [_numberLabel.centerYAnchor
        constraintEqualToAnchor:_numberContainerView.centerYAnchor],

    [_contentLabel.leadingAnchor
        constraintEqualToAnchor:_numberContainerView.trailingAnchor
                       constant:kNumberIconPadding],
    [_contentLabel.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor
                       constant:-kCellXPadding],
    [_contentLabel.topAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor],
    [_contentLabel.bottomAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor
                       constant:-kCellYPadding],
    [_contentLabel.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_numberContainerView.heightAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

- (void)setContentText:(NSString*)text number:(NSInteger)number {
  self.accessibilityLabel = text;
  _contentLabel.text = text;
  _numberLabel.text = [@(number) stringValue];
}

@end
