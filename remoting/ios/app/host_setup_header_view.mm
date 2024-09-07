// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/host_setup_header_view.h"

#import <MaterialComponents/MaterialTypography.h>

#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/view_utils.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kSetupTitleInset = 22.f;
static const CGFloat kYPadding = 28.f;

@implementation HostSetupHeaderView

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  if ((self = [super initWithStyle:style reuseIdentifier:reuseIdentifier])) {
    [self commonInit];
  }
  return self;
}

- (void)commonInit {
  NSString* titleText = l10n_util::GetNSString(IDS_HOST_SETUP_TITLE);
  self.isAccessibilityElement = YES;
  self.accessibilityLabel = titleText;
  self.backgroundColor = RemotingTheme.setupListBackgroundColor;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = titleText;
  titleLabel.font = MDCTypography.titleFont;
  titleLabel.numberOfLines = 1;
  titleLabel.adjustsFontSizeToFitWidth = YES;
  titleLabel.textColor = RemotingTheme.setupListTextColor;
  [self.contentView addSubview:titleLabel];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  UILayoutGuide* safeAreaLayoutGuide =
      remoting::SafeAreaLayoutGuideForView(self.contentView);
  [NSLayoutConstraint activateConstraints:@[
    [titleLabel.topAnchor constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor
                                         constant:kYPadding],
    [titleLabel.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:kSetupTitleInset],
    [titleLabel.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor
                       constant:-kSetupTitleInset],
    [titleLabel.bottomAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor
                       constant:-kYPadding],
  ]];
}

@end
