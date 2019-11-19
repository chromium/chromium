// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"

#include "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kShiftTilesDownAnimationDuration = 0.2;
const CGFloat kShiftTilesUpAnimationDuration = 0.25;
}  // namespace

@interface ContentSuggestionsHeaderSynchronizer ()<UIGestureRecognizerDelegate>

@property(nonatomic, weak, readonly) UICollectionView* collectionView;
// |YES| if the fakebox header should be animated on scroll.
@property(nonatomic, assign) BOOL shouldAnimateHeader;
@property(nonatomic, weak) id<ContentSuggestionsCollectionControlling>
    collectionController;
@property(nonatomic, weak) id<ContentSuggestionsHeaderControlling>
    headerController;
@property(nonatomic, assign) CFTimeInterval shiftTileStartTime;

// Tap gesture recognizer when the omnibox is focused.
@property(nonatomic, strong) UITapGestureRecognizer* tapGestureRecognizer;
@end

@implementation ContentSuggestionsHeaderSynchronizer

@synthesize collectionController = _collectionController;
@synthesize headerController = _headerController;
@synthesize shouldAnimateHeader = _shouldAnimateHeader;
@synthesize shiftTileStartTime = _shiftTileStartTime;
@synthesize tapGestureRecognizer = _tapGestureRecognizer;
@synthesize collectionShiftingOffset = _collectionShiftingOffset;

- (instancetype)
initWithCollectionController:
    (id<ContentSuggestionsCollectionControlling>)collectionController
            headerController:
                (id<ContentSuggestionsHeaderControlling>)headerController {
  self = [super init];
  if (self) {
    _shiftTileStartTime = -1;
    _shouldAnimateHeader = YES;

    _tapGestureRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(unfocusOmnibox)];
    [_tapGestureRecognizer setDelegate:self];

    _headerController = headerController;
    _collectionController = collectionController;

    _headerController.collectionSynchronizer = self;
    _collectionController.headerSynchronizer = self;

    _collectionShiftingOffset = 0;
  }
  return self;
}

#pragma mark - ContentSuggestionsCollectionSynchronizing

- (void)shiftTilesDown {
  [self.collectionView removeGestureRecognizer:self.tapGestureRecognizer];

  self.shouldAnimateHeader = YES;

  if (self.collectionShiftingOffset == 0 || self.collectionView.dragging) {
    self.collectionShiftingOffset = 0;
    [self updateFakeOmniboxOnCollectionScroll];
    return;
  }

  self.collectionController.scrolledToTop = NO;

  // CADisplayLink is used for this animation instead of the standard UIView
  // animation because the standard animation did not properly convert the
  // fakebox from its scrolled up mode to its scrolled down mode. Specifically,
  // calling |UICollectionView reloadData| adjacent to the standard animation
  // caused the fakebox's views to jump incorrectly. CADisplayLink avoids this
  // problem because it allows |shiftTilesDownAnimationDidFire| to directly
  // control each frame.
  CADisplayLink* link = [CADisplayLink
      displayLinkWithTarget:self
                   selector:@selector(shiftTilesDownAnimationDidFire:)];
  [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)shiftTilesUpWithAnimations:(ProceduralBlock)animations
                        completion:(ProceduralBlock)completion {
  // Add gesture recognizer to collection view when the omnibox is focused.
  [self.collectionView addGestureRecognizer:self.tapGestureRecognizer];

  if (self.collectionView.decelerating) {
    // Stop the scrolling if the scroll view is decelerating to prevent the
    // focus to be immediately lost.
    [self.collectionView setContentOffset:self.collectionView.contentOffset
                                 animated:NO];
  }

  if (self.collectionController.scrolledToTop) {
    self.shouldAnimateHeader = NO;
    if (completion)
      completion();
    return;
  }

  if (CGSizeEqualToSize(self.collectionView.contentSize, CGSizeZero))
    [self.collectionView layoutIfNeeded];

  CGFloat pinnedOffsetY = [self.headerController pinnedOffsetY];
  self.collectionShiftingOffset =
      MAX(0, pinnedOffsetY - self.collectionView.contentOffset.y);

  self.collectionController.scrolledToTop = YES;
  self.shouldAnimateHeader = YES;

  [UIView animateWithDuration:kShiftTilesUpAnimationDuration
      animations:^{
        if (self.collectionView.contentOffset.y < pinnedOffsetY) {
          if (animations)
            animations();
          // Changing the contentOffset of the collection results in a scroll
          // and a change in the constraints of the header.
          self.collectionView.contentOffset = CGPointMake(0, pinnedOffsetY);
          // Layout the header for the constraints to be animated.
          [self.headerController layoutHeader];
          [self.collectionView.collectionViewLayout invalidateLayout];
        }
      }
      completion:^(BOOL finished) {
        // Check to see if the collection are still scrolled to the top -- it's
        // possible (and difficult) to unfocus the omnibox and initiate a
        // -shiftTilesDown before the animation here completes.
        if (self.collectionController.scrolledToTop) {
          self.shouldAnimateHeader = NO;
          if (completion)
            completion();
        }
      }];
}

- (void)invalidateLayout {
  [self updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark - ContentSuggestionsHeaderSynchronizing

- (void)updateFakeOmniboxOnCollectionScroll {
  // Unfocus the omnibox when the scroll view is scrolled by the user (but not
  // when a scroll is triggered by layout/UIKit).
  if ([self.headerController isOmniboxFocused] && !self.shouldAnimateHeader &&
      self.collectionView.dragging) {
    [self.headerController unfocusOmnibox];
  }

  if (self.shouldAnimateHeader) {
    UIEdgeInsets insets = self.collectionView.safeAreaInsets;
    [self.headerController
        updateFakeOmniboxForOffset:self.collectionView.contentOffset.y
                       screenWidth:self.collectionView.frame.size.width
                    safeAreaInsets:insets];
  }
}

- (void)updateFakeOmniboxOnNewWidth:(CGFloat)width {
  if (self.shouldAnimateHeader) {
    // We check -superview here because in certain scenarios (such as when the
    // VC is rotated underneath another presented VC), in a
    // UICollectionViewController -viewSafeAreaInsetsDidChange the VC.view has
    // updated safeAreaInsets, but VC.collectionView does not until a layer
    // -viewDidLayoutSubviews.  Since self.collectionView and it's superview
    // should always have the same safeArea, this should be safe.
    UIEdgeInsets insets = self.collectionView.superview.safeAreaInsets;
    [self.headerController
        updateFakeOmniboxForOffset:self.collectionView.contentOffset.y
                       screenWidth:width
                    safeAreaInsets:insets];
  } else {
    [self.headerController updateFakeOmniboxForWidth:width];
  }
}

- (void)updateConstraints {
  [self.headerController updateConstraints];
}

- (void)unfocusOmnibox {
  [self.headerController unfocusOmnibox];
}

- (CGFloat)pinnedOffsetY {
  return [self.headerController pinnedOffsetY];
}

- (CGFloat)headerHeight {
  return [self.headerController headerHeight];
}

- (void)setShowing:(BOOL)showing {
  self.headerController.showing = showing;
}

- (BOOL)isShowing {
  return self.headerController.isShowing;
}

#pragma mark - Private

// Convenience method to get the collection view of the suggestions.
- (UICollectionView*)collectionView {
  return [self.collectionController collectionView];
}

// Updates the collection view's scroll view offset for the next frame of the
// shiftTilesDown animation.
- (void)shiftTilesDownAnimationDidFire:(CADisplayLink*)link {
  // If this is the first frame of the animation, store the starting timestamp
  // and do nothing.
  if (self.shiftTileStartTime == -1) {
    self.shiftTileStartTime = link.timestamp;
    return;
  }

  CFTimeInterval timeElapsed = link.timestamp - self.shiftTileStartTime;
  double percentComplete = timeElapsed / kShiftTilesDownAnimationDuration;
  // Ensure that the percentage cannot be above 1.0.
  if (percentComplete > 1.0)
    percentComplete = 1.0;

  // Find how much the collection view should be scrolled up in the next frame.
  CGFloat yOffset =
      (1.0 - percentComplete) * [self.headerController pinnedOffsetY] +
      percentComplete * MAX([self.headerController pinnedOffsetY] -
                                self.collectionShiftingOffset,
                            0);
  self.collectionView.contentOffset = CGPointMake(0, yOffset);

  if (percentComplete == 1.0) {
    [link invalidate];
    self.collectionShiftingOffset = 0;
    // Reset |shiftTileStartTime| to its sentinel value.
    self.shiftTileStartTime = -1;
  }
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  BOOL isMostVisitedCell =
      content_suggestions::nearestAncestor(
          touch.view, [ContentSuggestionsMostVisitedCell class]) != nil;
  BOOL isMostVisitedActionCell =
      content_suggestions::nearestAncestor(
          touch.view, [ContentSuggestionsMostVisitedActionCell class]) != nil;
  BOOL isSuggestionCell =
      content_suggestions::nearestAncestor(
          touch.view, [ContentSuggestionsCell class]) != nil;
  return !isMostVisitedCell && !isMostVisitedActionCell && !isSuggestionCell;
}

- (UIView*)nearestAncestorOfView:(UIView*)view withClass:(Class)aClass {
  if (!view) {
    return nil;
  }
  if ([view isKindOfClass:aClass]) {
    return view;
  }
  return [self nearestAncestorOfView:[view superview] withClass:aClass];
}

@end
