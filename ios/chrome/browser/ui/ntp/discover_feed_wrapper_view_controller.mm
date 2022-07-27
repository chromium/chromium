// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_wrapper_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation DiscoverFeedWrapperViewController {
  __weak id<DiscoverFeedWrapperViewControllerDelegate> _delegate;
}

- (instancetype)initWithDelegate:
                    (id<DiscoverFeedWrapperViewControllerDelegate>)delegate
      discoverFeedViewController:(UIViewController*)discoverFeed {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    if (discoverFeed) {
      _discoverFeed = discoverFeed;
      _delegate = delegate;

      // Iterates through subviews to find collection view containing feed
      // articles.
      // TODO(crbug.com/1085419): Once the CollectionView is cleanly exposed,
      // remove this loop.
      for (UIView* view in _discoverFeed.view.subviews) {
        if ([view isKindOfClass:[UICollectionView class]]) {
          _contentCollectionView = static_cast<UICollectionView*>(view);
        }
      }
      DCHECK(_contentCollectionView);
    }
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Configure appropriate collection view based on feed visibility. If
  // |discoverFeed| exists, then the feed must be enabled and visible.
  if (self.discoverFeed && self.contentCollectionView) {
    [self configureDiscoverFeedAsWrapper];
    [_delegate updateTheme];
  } else {
    [self configureEmptyCollectionAsWrapper];
  }
}

#pragma mark - Private

// If the feed is visible, we configure the feed's collection view and view
// controller to be used in the NTP.
- (void)configureDiscoverFeedAsWrapper {
  UIView* discoverView = self.discoverFeed.view;
  [self.discoverFeed willMoveToParentViewController:self];
  [self addChildViewController:self.discoverFeed];
  [self.view addSubview:discoverView];
  [self.discoverFeed didMoveToParentViewController:self];
  discoverView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(discoverView, self.view);
}

// If the feed is not visible, we prepare the empty collection view to be used
// in the NTP.
- (void)configureEmptyCollectionAsWrapper {
  UICollectionViewLayout* layout = [[UICollectionViewLayout alloc] init];
  UICollectionView* emptyCollectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:layout];
  [emptyCollectionView setShowsVerticalScrollIndicator:NO];
  [self.view addSubview:emptyCollectionView];
  self.contentCollectionView = emptyCollectionView;
  self.contentCollectionView.backgroundColor =
      IsContentSuggestionsUIModuleRefreshEnabled()
          ? [UIColor clearColor]
          : ntp_home::kNTPBackgroundColor();
  self.contentCollectionView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.contentCollectionView, self.view);
}

@end
