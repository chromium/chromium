// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/discover_feed_wrapper_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation DiscoverFeedWrapperViewController

- (instancetype)initWithDiscoverFeedViewController:
    (UIViewController*)discoverFeed {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    // TODO(crbug.com/1114792): Handle case where feed is disabled, replacing it
    // with a regular scroll view.
    _discoverFeed = discoverFeed;

    // Iterates through subviews to find collection view containing feed
    // articles.
    // TODO(crbug.com/1085419): Once the CollectionView is cleanly exposed,
    // remove this loop.
    for (UIView* view in _discoverFeed.view.subviews) {
      if ([view isKindOfClass:[UICollectionView class]]) {
        _feedCollectionView = static_cast<UICollectionView*>(view);
      }
    }
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* discoverView = self.discoverFeed.view;
  [self.discoverFeed willMoveToParentViewController:self];
  [self addChildViewController:self.discoverFeed];
  [self.view addSubview:discoverView];
  [self.discoverFeed didMoveToParentViewController:self];
  discoverView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(discoverView, self.view);
}

@end
