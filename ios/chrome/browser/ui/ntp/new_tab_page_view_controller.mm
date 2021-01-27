// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_layout.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The offset from the bottom of the content suggestions header before changing
// ownership of the fake omnibox. This value can be a large range of numbers, as
// long as it is larger than the omnibox height (plus an addional offset to make
// it look smooth). Otherwise, the omnibox hides beneath the feed before
// changing ownership.
const CGFloat kOffsetToPinOmnibox = 100;
// Offset so the FakeOmnibox owned by this ViewController completely covers the
// top of the screen.
const CGFloat kFakeOmniboxTopOffset = 3;
}

@interface NewTabPageViewController ()

// View controller representing the NTP content suggestions. These suggestions
// include the most visited site tiles, the shortcut tiles, the fake omnibox and
// the Google doodle.
@property(nonatomic, strong)
    UICollectionViewController* contentSuggestionsViewController;

// The overscroll actions controller managing accelerators over the toolbar.
@property(nonatomic, strong)
    OverscrollActionsController* overscrollActionsController;

// Whether or not the user has scrolled into the feed, transferring ownership of
// the omnibox to allow it to stick to the top of the NTP.
@property(nonatomic, assign, getter=isScrolledIntoFeed) BOOL scrolledIntoFeed;

// The collection view layout for the uppermost content suggestions collection
// view.
@property(nonatomic, weak) ContentSuggestionsLayout* contentSuggestionsLayout;

// Content suggestions collection view height for setting the initial NTP offset
// to be the top of the page. If value is |NAN|, then the offset was calculated
// from the saved web state instead.
@property(nonatomic, assign) CGFloat initialContentOffsetFromContentSuggestions;

@end

@implementation NewTabPageViewController

// Synthesized for ContentSuggestionsCollectionControlling protocol.
@synthesize headerSynchronizer = _headerSynchronizer;
@synthesize scrolledToTop = _scrolledToTop;

- (instancetype)initWithContentSuggestionsViewController:
    (UICollectionViewController*)contentSuggestionsViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _contentSuggestionsViewController = contentSuggestionsViewController;
    // TODO(crbug.com/1114792): Instantiate this depending on the initial scroll
    // position.
    // TODO(crbug.com/1114792): Stick the fake omnibox based on default scroll
    // position.
    _scrolledIntoFeed = NO;
    _initialContentOffsetFromContentSuggestions = 0;
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

  self.view.backgroundColor = ntp_home::kNTPBackgroundColor();

  _contentSuggestionsLayout = static_cast<ContentSuggestionsLayout*>(
      self.contentSuggestionsViewController.collectionView
          .collectionViewLayout);
  _contentSuggestionsLayout.isScrolledIntoFeed = self.isScrolledIntoFeed;
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // The scroll position should not be set if
  // |initialContentOffsetFromContentSuggestions| is NaN, because this means
  // that it was already set from the saved web state. The scroll position
  // should only be adjutsed until the feed inset is correctly set, because this
  // signifies that the view has appeared.
  if (!isnan(self.initialContentOffsetFromContentSuggestions) &&
      self.discoverFeedWrapperViewController.feedCollectionView.contentInset
              .top != [self adjustedContentSuggestionsHeight]) {
    [self setContentOffset:-[self adjustedContentSuggestionsHeight]
            fromSavedState:NO];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  [self updateFeedInsetsForContentSuggestions];

  self.headerSynchronizer.showing = YES;
  // Reload data to ensure the Most Visited tiles and fake omnibox are correctly
  // positioned, in particular during a rotation while a ViewController is
  // presented in front of the NTP.
  [self.headerSynchronizer
      updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.contentSuggestionsViewController.collectionView
          .collectionViewLayout invalidateLayout];
  // Ensure initial fake omnibox layout.
  [self.headerSynchronizer updateFakeOmniboxOnCollectionScroll];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  self.headerSynchronizer.showing = NO;
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];

  // Only get the bottom safe area inset.
  UIEdgeInsets insets = UIEdgeInsetsZero;
  insets.bottom = self.view.safeAreaInsets.bottom;
  self.discoverFeedWrapperViewController.feedCollectionView.contentInset =
      insets;

  [self.headerSynchronizer
      updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.headerSynchronizer updateConstraints];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  void (^alongsideBlock)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.headerSynchronizer updateFakeOmniboxOnNewWidth:size.width];
        [self.contentSuggestionsViewController.collectionView
                .collectionViewLayout invalidateLayout];
      };
  [coordinator animateAlongsideTransition:alongsideBlock completion:nil];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self.contentSuggestionsViewController.collectionView
            .collectionViewLayout invalidateLayout];
    [self.headerSynchronizer updateFakeOmniboxOnCollectionScroll];
  }
  [self.headerSynchronizer updateConstraints];
  [self updateOverscrollActionsState];
}

#pragma mark - Public

- (void)willUpdateSnapshot {
  [self.overscrollActionsController clear];
}

- (void)setContentOffset:(CGFloat)offset {
  [self setContentOffset:offset fromSavedState:YES];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // Scroll events should not be handled until the content suggestions have been
  // layed out.
  if (!self.contentSuggestionsViewController.collectionView.contentSize
           .height) {
    return;
  }
  [self.overscrollActionsController scrollViewDidScroll:scrollView];
  [self.headerSynchronizer updateFakeOmniboxOnCollectionScroll];
  self.scrolledToTop =
      scrollView.contentOffset.y >= [self.headerSynchronizer pinnedOffsetY];
  // Fixes the content suggestions collection view layout so that the header
  // scrolls at the same rate as the rest.
  if (scrollView.contentOffset.y > -self.contentSuggestionsViewController
                                        .collectionView.contentSize.height) {
    [self.contentSuggestionsViewController.collectionView
            .collectionViewLayout invalidateLayout];
  }
  // Changes ownership of fake omnibox view based on scroll position.
  if (!self.isScrolledIntoFeed &&
      scrollView.contentOffset.y > -kOffsetToPinOmnibox) {
    [self stickFakeOmniboxToTop];
  } else if (self.isScrolledIntoFeed &&
             scrollView.contentOffset.y <= -kOffsetToPinOmnibox) {
    [self resetFakeOmnibox];
  }
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
  // User has tapped the status bar to scroll to the top.
  // Prevent scrolling back to pre-focus state, making sure we don't have
  // two scrolling animations running at the same time.
  [self.headerSynchronizer resetPreFocusOffset];
  // Unfocus omnibox without scrolling back.
  [self.headerSynchronizer unfocusOmnibox];
  return YES;
}

#pragma mark - ContentSuggestionsCollectionControlling

- (UICollectionView*)collectionView {
  return self.discoverFeedWrapperViewController.feedCollectionView;
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

// Lets this view own the fake omnibox and sticks it to the top of the NTP.
- (void)stickFakeOmniboxToTop {
  [self setIsScrolledIntoFeed:YES];

  [self.headerController removeFromParentViewController];
  [self.headerController.view removeFromSuperview];

  // If |self.headerController| is nil after removing it from the view hierarchy
  // it means its no longer owned by anyone (e.g. The coordinator might have
  // been stopped.) and we shouldn't try to add it again.
  if (!self.headerController)
    return;

  [self.view addSubview:self.headerController.view];

  [NSLayoutConstraint activateConstraints:@[
    [self.headerController.view.topAnchor
        constraintEqualToAnchor:self.discoverFeedWrapperViewController.view
                                    .topAnchor
                       constant:-([self.ntpContentDelegate
                                        heightAboveFakeOmnibox]) -
                                kFakeOmniboxTopOffset],
    [self.headerController.view.leadingAnchor
        constraintEqualToAnchor:self.discoverFeedWrapperViewController.view
                                    .leadingAnchor],
    [self.headerController.view.trailingAnchor
        constraintEqualToAnchor:self.discoverFeedWrapperViewController.view
                                    .trailingAnchor],
    [self.headerController.view.heightAnchor
        constraintEqualToConstant:self.headerController.view.frame.size.height],
  ]];
}

// Gives content suggestions collection view ownership of the fake omnibox for
// the width animation.
- (void)resetFakeOmnibox {
  [self setIsScrolledIntoFeed:NO];

  [self.headerController removeFromParentViewController];
  [self.headerController.view removeFromSuperview];

  // Reload the content suggestions so that the fake omnibox goes back where it
  // belongs. This can probably be optimized by just reloading the header, if
  // that doesn't mess up any collection/header interactions.
  [self.ntpContentDelegate reloadContentSuggestions];
}

// Sets the feed collection contentOffset to |offset| to set the initial scroll
// position. If |fromSavedState| is NO, then the offset is set from the content
// suggestions collection height. If |fromSavedState| is YES, then the offset is
// forcefully set from a different source (like the cached navigation scroll
// position).
- (void)setContentOffset:(CGFloat)offset fromSavedState:(BOOL)isFromSavedState {
  self.discoverFeedWrapperViewController.feedCollectionView.contentOffset =
      CGPointMake(0, offset);
  self.initialContentOffsetFromContentSuggestions =
      isFromSavedState ? NAN : offset;
  self.scrolledIntoFeed =
      self.discoverFeedWrapperViewController.feedCollectionView.contentOffset
          .y > kOffsetToPinOmnibox;
}

// Sets an inset to the Discover feed equal to the content suggestions height,
// so that the content suggestions could act as the feed header.
- (void)updateFeedInsetsForContentSuggestions {
  CGFloat contentSuggestionsHeight =
      self.contentSuggestionsViewController.collectionView.contentSize.height;
  // TODO(crbug.com/1114792): Handle landscape/iPad layout.
  self.contentSuggestionsViewController.view.frame =
      CGRectMake(0, -contentSuggestionsHeight, self.view.frame.size.width,
                 contentSuggestionsHeight);
  self.discoverFeedWrapperViewController.feedCollectionView.contentInset =
      UIEdgeInsetsMake([self adjustedContentSuggestionsHeight], 0, 0, 0);
  self.headerSynchronizer.additionalOffset = contentSuggestionsHeight;
}

// Content suggestions height adjusted with the safe area top insets.
- (CGFloat)adjustedContentSuggestionsHeight {
  return self.contentSuggestionsViewController.collectionView.contentSize
             .height +
         self.view.safeAreaInsets.top;
}

#pragma mark - Setters

// Sets whether or not the NTP is scrolled into the feed and notifies the
// content suggestions layout to avoid it changing the omnibox frame when this
// view controls its position.
- (void)setIsScrolledIntoFeed:(BOOL)scrolledIntoFeed {
  _scrolledIntoFeed = scrolledIntoFeed;
  self.contentSuggestionsLayout.isScrolledIntoFeed = scrolledIntoFeed;
}

@end
