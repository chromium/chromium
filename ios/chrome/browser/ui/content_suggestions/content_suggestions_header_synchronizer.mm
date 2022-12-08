// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
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
// `YES` if the fakebox header should be animated on scroll.
@property(nonatomic, assign) BOOL shouldAnimateHeader;
@property(nonatomic, weak) id<ContentSuggestionsCollectionControlling>
    collectionController;
@property(nonatomic, weak) id<ContentSuggestionsHeaderControlling>
    headerController;
@property(nonatomic, assign) CFTimeInterval shiftTileStartTime;

// Tap gesture recognizer when the omnibox is focused.
@property(nonatomic, strong) UITapGestureRecognizer* tapGestureRecognizer;
// Animator for the shiftTilesUp animation.
@property(nonatomic, strong) UIViewPropertyAnimator* animator;
@end

@implementation ContentSuggestionsHeaderSynchronizer

@synthesize collectionController = _collectionController;
@synthesize headerController = _headerController;
@synthesize shouldAnimateHeader = _shouldAnimateHeader;
@synthesize shiftTileStartTime = _shiftTileStartTime;
@synthesize tapGestureRecognizer = _tapGestureRecognizer;
@synthesize collectionShiftingOffset = _collectionShiftingOffset;
// Synthesized for ContentSuggestionsSynchronizing protocol.
@synthesize additionalOffset = _additionalOffset;

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

    _collectionShiftingOffset = 0;
    _additionalOffset = 0;
  }
  return self;
}

#pragma mark - ContentSuggestionsCollectionSynchronizing

- (void)shiftTilesDown {
  [self.collectionView removeGestureRecognizer:self.tapGestureRecognizer];

  self.shouldAnimateHeader = YES;

  if (self.animator.running) {
    [self.animator stopAnimation:NO];
    [self.animator finishAnimationAtPosition:UIViewAnimatingPositionStart];
    self.animator = nil;
  }

  if (self.collectionShiftingOffset == 0 || self.collectionView.dragging) {
    self.collectionShiftingOffset = 0;
    [self updateFakeOmniboxForScrollPosition];
    return;
  }

  self.collectionController.scrolledToMinimumHeight = NO;

  // CADisplayLink is used for this animation instead of the standard UIView
  // animation because the standard animation did not properly convert the
  // fakebox from its scrolled up mode to its scrolled down mode. Specifically,
  // calling `UICollectionView reloadData` adjacent to the standard animation
  // caused the fakebox's views to jump incorrectly. CADisplayLink avoids this
  // problem because it allows `shiftTilesDownAnimationDidFire` to directly
  // control each frame.
  CADisplayLink* link = [CADisplayLink
      displayLinkWithTarget:self
                   selector:@selector(shiftTilesDownAnimationDidFire:)];
  [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)shiftTilesUpWithAnimations:(ProceduralBlock)animations
                        completion:
                            (void (^)(UIViewAnimatingPosition))completion {
  // Add gesture recognizer to collection view when the omnibox is focused.
  [self.collectionView addGestureRecognizer:self.tapGestureRecognizer];

  if (self.collectionView.decelerating) {
    // Stop the scrolling if the scroll view is decelerating to prevent the
    // focus to be immediately lost.
    [self.collectionView setContentOffset:self.collectionView.contentOffset
                                 animated:NO];
  }

  if (self.collectionController.scrolledToMinimumHeight) {
    self.shouldAnimateHeader = NO;
    if (completion)
      completion(UIViewAnimatingPositionEnd);
    return;
  }

  if (CGSizeEqualToSize(self.collectionView.contentSize, CGSizeZero))
    [self.collectionView layoutIfNeeded];

  CGFloat pinnedOffsetY = [self.headerController pinnedOffsetY];
  self.collectionShiftingOffset =
      MAX(-self.additionalOffset, pinnedOffsetY - [self adjustedOffset].y);
  self.shouldAnimateHeader = YES;

  CGFloat pinnedOffsetBeforeAnimation = [self pinnedOffsetY];
  __weak __typeof(self) weakSelf = self;

  ProceduralBlock shiftOmniboxToTop = ^{
    __typeof(weakSelf) strongSelf = weakSelf;
    // Changing the contentOffset of the collection results in a
    // scroll and a change in the constraints of the header.
    strongSelf.collectionView.contentOffset =
        CGPointMake(0, [strongSelf pinnedOffsetY]);
    // Layout the header for the constraints to be animated.
    [strongSelf.headerController layoutHeader];
    [strongSelf.collectionView.collectionViewLayout invalidateLayout];
  };

  self.animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:kShiftTilesUpAnimationDuration
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              if (!weakSelf) {
                return;
              }

              __typeof(weakSelf) strongSelf = weakSelf;
              if (strongSelf.collectionView.contentOffset.y <
                  [self pinnedOffsetY]) {
                if (animations)
                  animations();
                shiftOmniboxToTop();
              }
            }];

  [self.animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    ContentSuggestionsHeaderSynchronizer* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    if (finalPosition == UIViewAnimatingPositionEnd) {
      // Content suggestion headers can be updated during the scroll, causing
      // `pinnedOffsetY` to be invalid. When this happens during the animation,
      // the tiles are not scrolled to the top causing the omnibox to be hidden
      // by the `PrimaryToolbarView`. In that state, the omnibox's popup and the
      // keyboard are still visible.
      // If the animation is not interrupted and `pinnedOffsetY` changed
      // during the animation, shift the omnibox to the top at the end of the
      // animation.
      if ([strongSelf pinnedOffsetY] != pinnedOffsetBeforeAnimation &&
          strongSelf.collectionView.contentOffset.y <
              [strongSelf pinnedOffsetY]) {
        shiftOmniboxToTop();
      }
      strongSelf.shouldAnimateHeader = NO;
    }

    strongSelf.collectionController.scrolledToMinimumHeight = YES;
    if (completion) {
      completion(finalPosition);
    }
  }];

  self.animator.interruptible = YES;
  [self.animator startAnimation];
}

- (void)invalidateLayout {
  [self updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.collectionView.collectionViewLayout invalidateLayout];

  dispatch_async(dispatch_get_main_queue(), ^{
    // On iOS 13, invalidating the layout doesn't reset the positioning of the
    // header. To make sure that it is correctly positioned, scroll 1pt. This
    // is done in the next runloop to have the collectionView resized and the
    // content offset set to the new value. See crbug.com/1025694.
    CGPoint currentOffset = [self.collectionView contentOffset];
    currentOffset.y += 1;
    [self.collectionView setContentOffset:currentOffset animated:YES];
  });
}

#pragma mark - ContentSuggestionsHeaderSynchronizing

- (BOOL)isOmniboxFocused {
  return [self.headerController isOmniboxFocused];
}

- (void)updateFakeOmniboxForScrollPosition {
  // Unfocus the omnibox when the scroll view is scrolled by the user (but not
  // when a scroll is triggered by layout/UIKit).
  if ([self.headerController isOmniboxFocused] && !self.shouldAnimateHeader &&
      self.collectionView.dragging) {
    [self.headerController unfocusOmnibox];
  }

  if (self.shouldAnimateHeader) {
    UIEdgeInsets insets = self.collectionView.safeAreaInsets;
    [self.headerController
        updateFakeOmniboxForOffset:[self adjustedOffset].y
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
    [self.headerController updateFakeOmniboxForOffset:[self adjustedOffset].y
                                          screenWidth:width
                                       safeAreaInsets:insets];
  } else {
    [self.headerController updateFakeOmniboxForWidth:width];
  }
}

- (void)updateConstraints {
  [self.headerController updateConstraints];
}

- (void)resetPreFocusOffset {
  self.collectionShiftingOffset = 0;
}

- (void)unfocusOmnibox {
  [self.headerController unfocusOmnibox];
}

- (CGFloat)pinnedOffsetY {
  return [self.headerController pinnedOffsetY] - self.additionalOffset;
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

#pragma mark - UIGestureRecognizerDelegate

- (UIView*)nearestAncestorOfView:(UIView*)view withClass:(Class)aClass {
  if (!view) {
    return nil;
  }
  if ([view isKindOfClass:aClass]) {
    return view;
  }
  return [self nearestAncestorOfView:[view superview] withClass:aClass];
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
  CGFloat yOffset = (1.0 - percentComplete) * [self pinnedOffsetY] +
                    percentComplete * MAX([self pinnedOffsetY] -
                                              self.collectionShiftingOffset,
                                          -self.additionalOffset);
  self.collectionView.contentOffset = CGPointMake(0, yOffset);

  if (percentComplete == 1.0) {
    [link invalidate];
    self.collectionShiftingOffset = 0;
    // Reset `shiftTileStartTime` to its sentinel value.
    self.shiftTileStartTime = -1;
  }
}

// Returns y-offset compensated for any additionalOffset that might be set.
- (CGPoint)adjustedOffset {
  CGPoint adjustedOffset = self.collectionView.contentOffset;
  adjustedOffset.y += self.additionalOffset;
  return adjustedOffset;
}

@end
