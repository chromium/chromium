// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/cells/status_item.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;
}  // namespace

@interface StatusCell ()
- (void)configureForState:(StatusItemState)state;
@end

@implementation StatusItem

@synthesize text = _text;
@synthesize state = _state;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [StatusCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(StatusCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text = self.text;
  [cell configureForState:self.state];
}

@end

@implementation StatusCell

@synthesize activityIndicator = _activityIndicator;
@synthesize verifiedImageView = _verifiedImageView;
@synthesize errorImageView = _errorImageView;
@synthesize textLabel = _textLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* contentView = self.contentView;

    // This view is used to center the graphic and the text label.
    UIView* verticalCenteringView = [[UIView alloc] init];
    verticalCenteringView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:verticalCenteringView];

    _activityIndicator = [[MDCActivityIndicator alloc] init];
    _activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];
    [_activityIndicator setRadius:10];
    _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    [verticalCenteringView addSubview:_activityIndicator];

    // TODO(crbug.com/508925): Get rid of IDR_IOS_CHECKMARK, use a vector icon
    // instead.
    _verifiedImageView =
        [[UIImageView alloc] initWithImage:NativeImage(IDR_IOS_CHECKMARK)];
    _verifiedImageView.contentMode = UIViewContentModeScaleAspectFit;
    _verifiedImageView.hidden = YES;
    _verifiedImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [verticalCenteringView addSubview:_verifiedImageView];

    _errorImageView =
        [[UIImageView alloc] initWithImage:NativeImage(IDR_IOS_ERROR)];
    _errorImageView.contentMode = UIViewContentModeScaleAspectFit;
    _errorImageView.hidden = YES;
    _errorImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [verticalCenteringView addSubview:_errorImageView];

    _textLabel = [[UILabel alloc] init];
    _textLabel.font = [[MDCTypography fontLoader] mediumFontOfSize:16];
    _textLabel.numberOfLines = 0;
    _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    // The label's position will be centered with Auto Layout but this ensures
    // that if there are multiple lines of text they will be centered relative
    // to each other.
    _textLabel.textAlignment = NSTextAlignmentCenter;
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [verticalCenteringView addSubview:_textLabel];

    const CGFloat kVerticalPadding = 16;
    const CGFloat kSpacing = 12;
    const CGFloat kActivityIndicatorSize = 24;
    [NSLayoutConstraint activateConstraints:@[
      [verticalCenteringView.centerXAnchor
          constraintEqualToAnchor:contentView.centerXAnchor],
      [verticalCenteringView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_activityIndicator.centerXAnchor
          constraintEqualToAnchor:verticalCenteringView.centerXAnchor],
      [_verifiedImageView.centerXAnchor
          constraintEqualToAnchor:verticalCenteringView.centerXAnchor],
      [_errorImageView.centerXAnchor
          constraintEqualToAnchor:verticalCenteringView.centerXAnchor],
      [_textLabel.centerXAnchor
          constraintEqualToAnchor:verticalCenteringView.centerXAnchor],
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_activityIndicator.bottomAnchor
          constraintEqualToAnchor:_textLabel.topAnchor
                         constant:-kSpacing],
      [_verifiedImageView.bottomAnchor
          constraintEqualToAnchor:_textLabel.topAnchor
                         constant:-kSpacing],
      [_errorImageView.bottomAnchor constraintEqualToAnchor:_textLabel.topAnchor
                                                   constant:-kSpacing],
      [_activityIndicator.heightAnchor
          constraintEqualToConstant:kActivityIndicatorSize],
      [verticalCenteringView.topAnchor
          constraintEqualToAnchor:_activityIndicator.topAnchor],
      [verticalCenteringView.bottomAnchor
          constraintEqualToAnchor:_textLabel.bottomAnchor],
      [verticalCenteringView.topAnchor
          constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                      constant:kVerticalPadding],
      [verticalCenteringView.bottomAnchor
          constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                   constant:-kVerticalPadding],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  [self configureForState:StatusItemState::VERIFYING];
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);
  self.textLabel.preferredMaxLayoutWidth = parentWidth - 2 * kHorizontalPadding;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - Private

- (void)configureForState:(StatusItemState)state {
  switch (state) {
    case StatusItemState::VERIFYING:
      self.textLabel.textColor = [UIColor colorNamed:kBlueColor];
      [self.activityIndicator startAnimating];
      self.activityIndicator.hidden = NO;
      self.verifiedImageView.hidden = YES;
      self.errorImageView.hidden = YES;
      break;
    case StatusItemState::VERIFIED:
      self.textLabel.textColor = [UIColor colorNamed:kBlueColor];
      [self.activityIndicator stopAnimating];
      self.activityIndicator.hidden = YES;
      self.verifiedImageView.hidden = NO;
      self.errorImageView.hidden = YES;
      break;
    case StatusItemState::ERROR:
      self.textLabel.textColor = [UIColor colorNamed:kRedColor];
      [self.activityIndicator stopAnimating];
      self.activityIndicator.hidden = YES;
      self.verifiedImageView.hidden = YES;
      self.errorImageView.hidden = NO;
      break;
  }
}

@end
