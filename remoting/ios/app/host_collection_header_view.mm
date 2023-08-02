// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/host_collection_header_view.h"

#import <UIKit/UIKit.h>

#import <MaterialComponents/MaterialTypography.h>

#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/view_utils.h"

// Applied on the left and right of the label.
static const float kTitleMargin = 12.f;

@interface HostCollectionHeaderView () {
 @private
  UILabel* _titleLabel;
}
@end

@implementation HostCollectionHeaderView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font = [MDCTypography body2Font];
    _titleLabel.textColor = RemotingTheme.hostListHeaderTitleColor;
    _titleLabel.backgroundColor = [UIColor clearColor];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_titleLabel];

    UILayoutGuide* safeAreaLayoutGuide =
        remoting::SafeAreaLayoutGuideForView(self);

    [NSLayoutConstraint activateConstraints:@[
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                         constant:kTitleMargin],
      [_titleLabel.centerYAnchor
          constraintEqualToAnchor:safeAreaLayoutGuide.centerYAnchor],
      [_titleLabel.trailingAnchor
          constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor
                         constant:-kTitleMargin],
    ]];
  }
  return self;
}

- (NSString*)text {
  return _titleLabel.text;
}

- (void)setText:(NSString*)text {
  _titleLabel.text = text;
  self.accessibilityLabel = text;
}

@end
