// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/discover_feed_preview/discover_feed_preview_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kURLBarMarginVertical = 13.0;
const CGFloat kURLBarMarginHorizontal = 16.0;
const CGFloat kSeparatorHeight = 0.1f;
}  // namespace

@interface DiscoverFeedPreviewViewController ()

// The view of the loaded webState.
@property(nonatomic, strong) UIView* webStateView;

// The preview url.
@property(nonatomic, copy) NSString* URL;

@end

@implementation DiscoverFeedPreviewViewController

- (instancetype)initWithView:(UIView*)webStateView URL:(NSString*)URL {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _webStateView = webStateView;
    _URL = URL;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UILabel* URLBarLabel = [[UILabel alloc] init];
  URLBarLabel.text = self.URL;
  URLBarLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  URLBarLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  URLBarLabel.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* URLBarView = [[UIView alloc] init];
  URLBarView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  URLBarView.translatesAutoresizingMaskIntoConstraints = NO;

  [URLBarView addSubview:URLBarLabel];
  [NSLayoutConstraint activateConstraints:@[
    [URLBarLabel.topAnchor constraintEqualToAnchor:URLBarView.topAnchor
                                          constant:kURLBarMarginVertical],
    [URLBarLabel.bottomAnchor constraintEqualToAnchor:URLBarView.bottomAnchor
                                             constant:-kURLBarMarginVertical],
    [URLBarLabel.leadingAnchor constraintEqualToAnchor:URLBarView.leadingAnchor
                                              constant:kURLBarMarginHorizontal],
    [URLBarLabel.trailingAnchor
        constraintEqualToAnchor:URLBarView.trailingAnchor
                       constant:-kURLBarMarginHorizontal],

  ]];

  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  separator.translatesAutoresizingMaskIntoConstraints = NO;

  self.webStateView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:URLBarView];
  [self.view addSubview:separator];
  [self.view addSubview:self.webStateView];

  [NSLayoutConstraint activateConstraints:@[
    [URLBarView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [URLBarView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [URLBarView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [URLBarView.bottomAnchor constraintEqualToAnchor:separator.topAnchor],
    [separator.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [separator.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.view.trailingAnchor],
    [separator.bottomAnchor
        constraintEqualToAnchor:self.webStateView.topAnchor],
    [separator.heightAnchor
        constraintEqualToConstant:ui::AlignValueToUpperPixel(kSeparatorHeight)],
    [self.webStateView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.webStateView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.webStateView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];
}

- (void)resetAutoLayoutForPreview {
  // Reset auto layout of the webStateView to YES, otherwise it can't be
  // expanded to a tab.
  self.webStateView.translatesAutoresizingMaskIntoConstraints = YES;
}

@end
