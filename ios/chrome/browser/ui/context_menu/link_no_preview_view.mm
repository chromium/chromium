// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/link_no_preview_view.h"

#import "ios/chrome/browser/ui/elements/favicon_container_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Margin at the top of the labels.
const CGFloat kTopMargin = 10;
// Margin at the bottom of the labels.
const CGFloat kBottomMargin = 7.5;
// Margin at the end of the labels.
const CGFloat kTrailingMargin = 4;
// Margin before the favicon.
const CGFloat kLeadingMargin = 15;
// Margin between the favicon and the text.
const CGFloat kFaviconToTextMargin = 14;

}  // namespace

@interface LinkNoPreviewView ()

@property(nonatomic, strong) UILabel* title;

@property(nonatomic, strong) UILabel* subtitle;

@property(nonatomic, strong) FaviconContainerView* faviconContainer;

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

    _faviconContainer = [[FaviconContainerView alloc] init];
    _faviconContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_faviconContainer];

    [NSLayoutConstraint activateConstraints:@[
      [_title.topAnchor constraintEqualToAnchor:self.topAnchor
                                       constant:kTopMargin],
      [_title.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                            constant:kTrailingMargin],

      [_title.bottomAnchor constraintEqualToAnchor:_subtitle.topAnchor],

      [_subtitle.leadingAnchor constraintEqualToAnchor:_title.leadingAnchor],
      [_subtitle.trailingAnchor constraintEqualToAnchor:_title.trailingAnchor],
      [_subtitle.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                             constant:-kBottomMargin],

      [_faviconContainer.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kLeadingMargin],
      [_faviconContainer.trailingAnchor
          constraintEqualToAnchor:_title.leadingAnchor
                         constant:-kFaviconToTextMargin],
      [_faviconContainer.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
    ]];
  }
  return self;
}

- (void)configureWithAttributes:(FaviconAttributes*)attributes {
  [self.faviconContainer.faviconView configureWithAttributes:attributes];
}

@end
