// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"

#import "ios/chrome/browser/ui/ntp/discover_feed_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageViewController ()

// View controller representing the Discover feed.
@property(nonatomic, strong)
    DiscoverFeedViewController* discoverFeedViewController;

// View controller representing the NTP content suggestions. These suggestions
// include the most visited site tiles, the shortcut tiles, the fake omnibox and
// the Google doodle.
@property(nonatomic, strong)
    UICollectionViewController* contentSuggestionsViewController;

@end

@implementation NewTabPageViewController

- (instancetype)initWithDiscoverFeedViewController:
                    (DiscoverFeedViewController*)discoverFeedViewController
                  contentSuggestionsViewController:
                      (UICollectionViewController*)
                          contentSuggestionsViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _discoverFeedViewController = discoverFeedViewController;
    _contentSuggestionsViewController = contentSuggestionsViewController;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* discoverFeedView = self.discoverFeedViewController.view;

  [self.discoverFeedViewController willMoveToParentViewController:self];
  [self addChildViewController:self.discoverFeedViewController];
  [self.view addSubview:discoverFeedView];
  [self.discoverFeedViewController didMoveToParentViewController:self];

  discoverFeedView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(discoverFeedView, self.view);

  [self.contentSuggestionsViewController
      willMoveToParentViewController:self.discoverFeedViewController
                                         .discoverFeed];
  [self.discoverFeedViewController.discoverFeed
      addChildViewController:self.contentSuggestionsViewController];
  [self.discoverFeedViewController.feedCollectionView
      addSubview:self.contentSuggestionsViewController.view];
  [self.contentSuggestionsViewController
      didMoveToParentViewController:self.discoverFeedViewController
                                        .discoverFeed];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Sets an inset to the Discover feed equal to the content suggestions height,
  // so that the content suggestions could act as the feed header.
  // TODO(crbug.com/1114792): Handle landscape/iPad layout.
  UICollectionView* collectionView =
      self.contentSuggestionsViewController.collectionView;
  self.contentSuggestionsViewController.view.frame =
      CGRectMake(0, -collectionView.contentSize.height,
                 self.view.frame.size.width, collectionView.contentSize.height);
  self.discoverFeedViewController.feedCollectionView.contentInset =
      UIEdgeInsetsMake(collectionView.contentSize.height, 0, 0, 0);
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewDidScrollToTop:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewWillBeginDecelerating:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (BOOL)scrollViewShouldScrollToTop:(UIScrollView*)scrollView {
  return NO;
}

@end
