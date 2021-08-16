// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/link_no_preview_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kTopMargin = 10;
const CGFloat kBottomMargin = 7.5;
const CGFloat kTrailingMargin = 4;

}  // namespace

@interface LinkNoPreviewView ()

@property(nonatomic, strong) UILabel* title;

@property(nonatomic, strong) UILabel* subtitle;

@end

@implementation LinkNoPreviewView

- (instancetype)initWithTitle:(NSString*)title subtitle:(NSString*)subtitle {
  self = [super init];
  if (self) {
    _title = [[UILabel alloc] init];
    _title.translatesAutoresizingMaskIntoConstraints = NO;
    _title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _title.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _title.text = title;
    [self addSubview:_title];

    _subtitle = [[UILabel alloc] init];
    _subtitle.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitle.text = subtitle;
    [self addSubview:_subtitle];

    [NSLayoutConstraint activateConstraints:@[
      [_title.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [_title.topAnchor constraintEqualToAnchor:self.topAnchor
                                       constant:kTopMargin],
      [_title.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                            constant:kTrailingMargin],

      [_title.bottomAnchor constraintEqualToAnchor:_subtitle.topAnchor],

      [_subtitle.leadingAnchor constraintEqualToAnchor:_title.leadingAnchor],
      [_subtitle.trailingAnchor constraintEqualToAnchor:_title.trailingAnchor],
      [_subtitle.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                             constant:-kBottomMargin],
    ]];
  }
  return self;
}

@end
