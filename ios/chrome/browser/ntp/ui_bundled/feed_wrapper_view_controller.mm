// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_wrapper_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation FeedWrapperViewController {
  __weak id<FeedWrapperViewControllerDelegate> _delegate;
}

- (instancetype)initWithDelegate:(id<FeedWrapperViewControllerDelegate>)delegate
              feedViewController:(UIViewController*)feedViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    if (feedViewController) {
      _feedViewController = feedViewController;
      _delegate = delegate;

      // Iterates through subviews to find collection view containing feed
      // articles.
      // TODO(crbug.com/40693626): Once the CollectionView is cleanly exposed,
      // remove this loop.
      for (UIView* view in _feedViewController.view.subviews) {
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
  // `feedViewController` exists, then the feed must be enabled and visible.
  if (self.feedViewController && self.contentCollectionView) {
    [self configureFeedAsWrapper];
    [_delegate updateTheme];
  } else {
    [self configureEmptyCollectionAsWrapper];
  }
}

#pragma mark - Public

- (NSUInteger)lastVisibleFeedCardIndex {
  DCHECK(self.contentCollectionView);
  NSArray<NSIndexPath*>* visibleCardIndices =
      [self.contentCollectionView indexPathsForVisibleItems];
  NSInteger lastVisibleIndex;
  for (NSIndexPath* cardIndex in visibleCardIndices) {
    lastVisibleIndex = MAX(lastVisibleIndex, cardIndex.item);
  }
  return lastVisibleIndex;
}

#pragma mark - Private

// If the feed is visible, we configure the feed's collection view and view
// controller to be used in the NTP.
- (void)configureFeedAsWrapper {
  UIView* feedView = self.feedViewController.view;
  [self.feedViewController willMoveToParentViewController:self];
  [self addChildViewController:self.feedViewController];
  [self.view addSubview:feedView];
  [self.feedViewController didMoveToParentViewController:self];
  feedView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(feedView, self.view.safeAreaLayoutGuide);
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
  self.contentCollectionView.backgroundColor = [UIColor clearColor];
  self.contentCollectionView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.contentCollectionView, self.view.safeAreaLayoutGuide);
}

@end
