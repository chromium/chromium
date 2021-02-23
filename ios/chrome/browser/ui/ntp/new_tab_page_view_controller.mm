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
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_omnibox_positioning.h"
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
}

@interface NewTabPageViewController () <NewTabPageOmniboxPositioning>

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

// Constraint to determine the height of the contained ContentSuggestions view.
@property(nonatomic, strong)
    NSLayoutConstraint* contentSuggestionsHeightConstraint;

// Array of constraints used to pin the fake Omnibox header into the top of the
// view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* fakeOmniboxConstraints;

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
  _contentSuggestionsLayout.omniboxPositioner = self;
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  [self updateContentSuggestionForCurrentLayout];
  [self updateHeaderSynchronizerOffset];
  [self.headerSynchronizer updateConstraints];
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

  // Set these constraints in viewWillAppear so ContentSuggestions View uses its
  // intrinsic height in the initial layout instead of
  // contentSuggestionsHeightConstraint. If this is not done the
  // ContentSuggestions View will look broken for a second before its properly
  // laid out.
  if (!self.contentSuggestionsHeightConstraint) {
    UIView* containerView =
        self.discoverFeedWrapperViewController.discoverFeed.view;
    UIView* contentSuggestionsView = self.contentSuggestionsViewController.view;
    contentSuggestionsView.translatesAutoresizingMaskIntoConstraints = NO;

    self.contentSuggestionsHeightConstraint =
        [contentSuggestionsView.heightAnchor
            constraintEqualToConstant:self.contentSuggestionsViewController
                                          .collectionView.contentSize.height];

    [NSLayoutConstraint activateConstraints:@[
      [self.discoverFeedWrapperViewController.feedCollectionView.topAnchor
          constraintEqualToAnchor:contentSuggestionsView.bottomAnchor],
      [containerView.safeAreaLayoutGuide.leadingAnchor
          constraintEqualToAnchor:contentSuggestionsView.leadingAnchor],
      [containerView.safeAreaLayoutGuide.trailingAnchor
          constraintEqualToAnchor:contentSuggestionsView.trailingAnchor],
      self.contentSuggestionsHeightConstraint,
    ]];
  }

  [self updateContentSuggestionForCurrentLayout];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Updates omnibox to ensure that the dimensions are correct when navigating
  // back to the NTP.
  [self.headerSynchronizer updateFakeOmniboxForScrollPosition];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  self.headerSynchronizer.showing = NO;
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];

  [self updateFeedInsetsForContentSuggestions];
  [self updateHeaderSynchronizerOffset];
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

- (void)willMoveToParentViewController:(UIViewController*)parent {
  if (self.parentViewController) {
    [self removeFromParentViewController];
  }
  [super willMoveToParentViewController:parent];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self.contentSuggestionsViewController.collectionView
            .collectionViewLayout invalidateLayout];
    [self.headerSynchronizer updateFakeOmniboxForScrollPosition];
  }
  [self.headerSynchronizer updateConstraints];
  [self updateOverscrollActionsState];
}

#pragma mark - Public

- (void)willUpdateSnapshot {
  [self.overscrollActionsController clear];
}

- (void)stopScrolling {
  UIScrollView* scrollView =
      self.discoverFeedWrapperViewController.feedCollectionView;
  [scrollView setContentOffset:scrollView.contentOffset animated:NO];
}

- (void)setContentOffset:(CGFloat)offset {
  [self setContentOffset:offset fromSavedState:YES];
}

- (void)updateLayoutForContentSuggestions {
  [self updateContentSuggestionForCurrentLayout];
}

- (CGFloat)contentSuggestionsContentHeight {
  return self.contentSuggestionsViewController.collectionView.contentSize
      .height;
}

- (void)handleDeviceRotation {
  [self.headerSynchronizer unfocusOmnibox];
  [self.contentSuggestionsViewController.collectionView
          .collectionViewLayout invalidateLayout];
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
  [self.headerSynchronizer updateFakeOmniboxForScrollPosition];
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

#pragma mark - NewTabPageOmniboxPositioning

- (CGFloat)stickyOmniboxHeight {
  // Takes the height of the entire header and subtracts the margin to stick the
  // fake omnibox. Adjusts this for the device by further subtracting the
  // toolbar height and safe area insets.
  return self.headerController.view.frame.size.height -
         ntp_header::kFakeOmniboxScrolledToTopMargin -
         ToolbarExpandedHeight(
             [UIApplication sharedApplication].preferredContentSizeCategory) -
         self.view.safeAreaInsets.top;
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

  self.fakeOmniboxConstraints = @[
    [self.headerController.view.topAnchor
        constraintEqualToAnchor:self.discoverFeedWrapperViewController.view
                                    .topAnchor
                       constant:-[self stickyOmniboxHeight]],
    [self.headerController.view.leadingAnchor
        constraintEqualToAnchor:self.discoverFeedWrapperViewController.view
                                    .leadingAnchor],
    [self.headerController.view.trailingAnchor
        constraintEqualToAnchor:self.discoverFeedWrapperViewController.view
                                    .trailingAnchor],
    [self.headerController.view.heightAnchor
        constraintEqualToConstant:self.headerController.view.frame.size.height],
  ];

  self.contentSuggestionsHeightConstraint.active = NO;
  [NSLayoutConstraint activateConstraints:self.fakeOmniboxConstraints];
}

// Gives content suggestions collection view ownership of the fake omnibox for
// the width animation.
- (void)resetFakeOmnibox {
  [self setIsScrolledIntoFeed:NO];

  [self.headerController removeFromParentViewController];
  [self.headerController.view removeFromSuperview];

  self.contentSuggestionsHeightConstraint.active = YES;
  [NSLayoutConstraint deactivateConstraints:self.fakeOmniboxConstraints];

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

// Updates the ContentSuggestionsViewController and its header for the current
// layout.
- (void)updateContentSuggestionForCurrentLayout {
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
  [self.headerSynchronizer updateFakeOmniboxForScrollPosition];
}

// Sets an inset to the Discover feed equal to the content suggestions height,
// so that the content suggestions could act as the feed header.
- (void)updateFeedInsetsForContentSuggestions {
  // TODO(crbug.com/1114792): Handle landscape/iPad layout.
  self.contentSuggestionsViewController.view.frame = CGRectMake(
      0, -[self contentSuggestionsContentHeight], self.view.frame.size.width,
      [self contentSuggestionsContentHeight]);
  self.discoverFeedWrapperViewController.feedCollectionView.contentInset =
      UIEdgeInsetsMake([self adjustedContentSuggestionsHeight], 0, 0, 0);
  self.contentSuggestionsHeightConstraint.constant =
      [self contentSuggestionsContentHeight];
  [self updateHeaderSynchronizerOffset];
}

// Updates headerSynchronizer's additionalOffset using the content suggestions
// content height and the safe area top insets.
- (void)updateHeaderSynchronizerOffset {
  self.headerSynchronizer.additionalOffset =
      [self contentSuggestionsContentHeight] + self.view.safeAreaInsets.top;
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
