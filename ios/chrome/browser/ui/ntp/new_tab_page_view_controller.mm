// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageViewController ()

// View controller representing the NTP content suggestions. These suggestions
// include the most visited site tiles, the shortcut tiles, the fake omnibox and
// the Google doodle.
@property(nonatomic, strong)
    UICollectionViewController* contentSuggestionsViewController;

// The overscroll actions controller managing accelerators over the toolbar.
@property(nonatomic, strong)
    OverscrollActionsController* overscrollActionsController;

@end

@implementation NewTabPageViewController

- (instancetype)initWithContentSuggestionsViewController:
    (UICollectionViewController*)contentSuggestionsViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _contentSuggestionsViewController = contentSuggestionsViewController;
  }

  return self;
}

- (void)dealloc {
  [self.overscrollActionsController invalidate];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  DCHECK(self.discoverFeedWrapperViewController);

  UIView* discoverFeedView = self.discoverFeedWrapperViewController.view;

  [self.discoverFeedWrapperViewController willMoveToParentViewController:self];
  [self addChildViewController:self.discoverFeedWrapperViewController];
  [self.view addSubview:discoverFeedView];
  [self.discoverFeedWrapperViewController didMoveToParentViewController:self];

  discoverFeedView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(discoverFeedView, self.view);

  [self.contentSuggestionsViewController
      willMoveToParentViewController:self.discoverFeedWrapperViewController
                                         .discoverFeed];
  [self.discoverFeedWrapperViewController.discoverFeed
      addChildViewController:self.contentSuggestionsViewController];
  [self.discoverFeedWrapperViewController.feedCollectionView
      addSubview:self.contentSuggestionsViewController.view];
  [self.contentSuggestionsViewController
      didMoveToParentViewController:self.discoverFeedWrapperViewController
                                        .discoverFeed];

  // Ensures that there is never any nested scrolling, since we are nesting the
  // content suggestions collection view in the feed collection view.
  self.contentSuggestionsViewController.collectionView.bounces = NO;
  self.contentSuggestionsViewController.collectionView.alwaysBounceVertical =
      NO;
  self.contentSuggestionsViewController.collectionView.scrollEnabled = NO;

  // Overscroll action does not work well with content offset, so set this
  // to never and internally offset the UI to account for safe area insets.
  self.discoverFeedWrapperViewController.feedCollectionView
      .contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;

  self.overscrollActionsController = [[OverscrollActionsController alloc]
      initWithScrollView:self.discoverFeedWrapperViewController
                             .feedCollectionView];
  [self.overscrollActionsController
      setStyle:OverscrollStyle::NTP_NON_INCOGNITO];
  self.overscrollActionsController.delegate = self.overscrollDelegate;
  [self updateOverscrollActionsState];
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
  self.discoverFeedWrapperViewController.feedCollectionView.contentInset =
      UIEdgeInsetsMake(collectionView.contentSize.height, 0, 0, 0);
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateOverscrollActionsState];
}

- (void)willUpdateSnapshot {
  [self.overscrollActionsController clear];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self.overscrollActionsController scrollViewDidScroll:scrollView];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [self.overscrollActionsController scrollViewWillBeginDragging:scrollView];
  // TODO(crbug.com/1114792): Add metrics recorder.
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  [self.overscrollActionsController
      scrollViewWillEndDragging:scrollView
                   withVelocity:velocity
            targetContentOffset:targetContentOffset];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  [self.overscrollActionsController scrollViewDidEndDragging:scrollView
                                              willDecelerate:decelerate];
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
  return YES;
}

#pragma mark - Private

// Enables or disables overscroll actions.
- (void)updateOverscrollActionsState {
  if (IsSplitToolbarMode(self)) {
    [self.overscrollActionsController enableOverscrollActions];
  } else {
    [self.overscrollActionsController disableOverscrollActions];
  }
}

@end
