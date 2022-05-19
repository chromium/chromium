// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section_view_controller.h"

#import "ios/chrome/browser/discover_feed/feed_constants.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FeedTopSectionViewController ()

// A vertical StackView which contains all the elements of the top section.
@property(nonatomic, strong) UIStackView* contentStack;

@end

@implementation FeedTopSectionViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.contentStack = [[UIStackView alloc] init];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  self.contentStack.axis = UILayoutConstraintAxisVertical;
  self.contentStack.distribution = UIStackViewDistributionFill;
  [self.view addSubview:self.contentStack];
  [self applyConstraints];
}

#pragma mark - Private

// Applies constraints.
- (void)applyConstraints {
  // Anchor container
  [NSLayoutConstraint activateConstraints:@[
    [self.contentStack.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.contentStack.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.contentStack.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.contentStack.widthAnchor
        constraintEqualToConstant:MIN(kDiscoverFeedContentWidth,
                                      self.view.frame.size.width)],
  ]];
}

@end
