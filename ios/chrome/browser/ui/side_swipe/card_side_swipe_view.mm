// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe/card_side_swipe_view.h"

#include <cmath>

#include "base/ios/device_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_util.h"
#import "ios/chrome/browser/ui/side_swipe/swipe_view.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/side_swipe_toolbar_snapshot_providing.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {
// Spacing between cards.
const CGFloat kCardHorizontalSpacing = 30;

// Portion of the screen an edge card can be dragged.
const CGFloat kEdgeCardDragPercentage = 0.35;

// Card animation times.
const NSTimeInterval kAnimationDuration = 0.15;

// Reduce size in -smallGreyImage by this factor.
const CGFloat kResizeFactor = 4;
}  // anonymous namespace

@interface CardSideSwipeView ()

// Pan touches ended or were cancelled.
- (void)finishPan;
// Is the current card is an edge card based on swipe direction.
- (BOOL)isEdgeSwipe;
// Initialize card based on model_'s webstatelist index.
- (void)setupCard:(SwipeView*)card withIndex:(int)index;
// Build a |kResizeFactor| sized greyscaled version of |image|.
- (UIImage*)smallGreyImage:(UIImage*)image;

@property(nonatomic, strong) NSLayoutConstraint* backgroundTopConstraint;
@end

@implementation CardSideSwipeView {
  // The direction of the swipe that initiated this horizontal view.
  UISwipeGestureRecognizerDirection _direction;

  // Card views currently displayed.
  SwipeView* _leftCard;
  SwipeView* _rightCard;

  // Most recent touch location.
  CGPoint currentPoint_;

  // Tab model.
  __weak TabModel* model_;
}

@synthesize backgroundTopConstraint = _backgroundTopConstraint;
@synthesize delegate = _delegate;
@synthesize topToolbarSnapshotProvider = _topToolbarSnapshotProvider;
@synthesize bottomToolbarSnapshotProvider = _bottomToolbarSnapshotProvider;
@synthesize topMargin = _topMargin;

- (instancetype)initWithFrame:(CGRect)frame
                    topMargin:(CGFloat)topMargin
                        model:(TabModel*)model {
  self = [super initWithFrame:frame];
  if (self) {
    model_ = model;
    currentPoint_ = CGPointZero;
    _topMargin = topMargin;

    UIView* background = [[UIView alloc] initWithFrame:CGRectZero];
    [self addSubview:background];

    [background setTranslatesAutoresizingMaskIntoConstraints:NO];
    CGFloat topInset = self.safeAreaInsets.top;
    self.backgroundTopConstraint =
        [[background topAnchor] constraintEqualToAnchor:self.topAnchor
                                               constant:-topInset];
    [NSLayoutConstraint activateConstraints:@[
      [[background rightAnchor] constraintEqualToAnchor:self.rightAnchor],
      [[background leftAnchor] constraintEqualToAnchor:self.leftAnchor],
      self.backgroundTopConstraint,
      [[background bottomAnchor] constraintEqualToAnchor:self.bottomAnchor]
    ]];

    background.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];

    _rightCard =
        [[SwipeView alloc] initWithFrame:CGRectZero topMargin:topMargin];
    _leftCard =
        [[SwipeView alloc] initWithFrame:CGRectZero topMargin:topMargin];
    [_rightCard setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_leftCard setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:_rightCard];
    [self addSubview:_leftCard];
    AddSameConstraints(_rightCard, self);
    AddSameConstraints(_leftCard, self);
  }
  return self;
}

- (void)setTopMargin:(CGFloat)topMargin {
  _topMargin = topMargin;
  _leftCard.topMargin = topMargin;
  _rightCard.topMargin = topMargin;
}

- (void)updateConstraints {
  [super updateConstraints];
  CGFloat topInset = self.safeAreaInsets.top;
  self.backgroundTopConstraint.constant = -topInset;
}

- (CGFloat)cardWidth {
  return CGRectGetWidth(self.bounds);
}

// Set up left and right card views depending on current WebState and swipe
// direction.
- (void)updateViewsForDirection:(UISwipeGestureRecognizerDirection)direction {
  _direction = direction;
  int currentIndex = model_.webStateList->active_index();
  CGFloat offset = UseRTLLayout() ? -1 : 1;
  if (_direction == UISwipeGestureRecognizerDirectionRight) {
    [self setupCard:_rightCard withIndex:currentIndex];
    [self setupCard:_leftCard withIndex:currentIndex - offset];
  } else {
    [self setupCard:_leftCard withIndex:currentIndex];
    [self setupCard:_rightCard withIndex:currentIndex + offset];
  }
}

- (UIImage*)smallGreyImage:(UIImage*)image {
  CGRect smallSize = CGRectMake(0, 0, image.size.width / kResizeFactor,
                                image.size.height / kResizeFactor);
  // Using CIFilter here on iOS 5+ might be faster, but it doesn't easily
  // allow for resizing.  At the max size, it's still too slow for side swipe.
  UIGraphicsBeginImageContextWithOptions(smallSize.size, YES, 0);
  [image drawInRect:smallSize blendMode:kCGBlendModeLuminosity alpha:1.0];
  UIImage* greyImage = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return greyImage;
}

// Create card view based on TabModel's WebStateList index.
- (void)setupCard:(SwipeView*)card withIndex:(int)index {
  if (index < 0 || index >= (NSInteger)[model_ count]) {
    [card setHidden:YES];
    return;
  }
  [card setHidden:NO];

  web::WebState* webState = model_.webStateList->GetWebStateAt(index);
  UIImage* topToolbarSnapshot = [self.topToolbarSnapshotProvider
      toolbarSideSwipeSnapshotForWebState:webState];
  [card setTopToolbarImage:topToolbarSnapshot];
  UIImage* bottomToolbarSnapshot = [self.bottomToolbarSnapshotProvider
      toolbarSideSwipeSnapshotForWebState:webState];
  [card setBottomToolbarImage:bottomToolbarSnapshot];

  // Converting snapshotted images to grey takes too much time for single core
  // devices.  Instead, show the colored image for single core devices and the
  // grey image for multi core devices.
  dispatch_queue_t priorityQueue =
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0ul);
  SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
      ^(UIImage* image) {
        if (PagePlaceholderTabHelper::FromWebState(webState)
                ->will_add_placeholder_for_next_navigation() &&
            !ios::device_util::IsSingleCoreDevice()) {
          [card setImage:nil];
          dispatch_async(priorityQueue, ^{
            UIImage* greyImage = [self smallGreyImage:image];
            dispatch_async(dispatch_get_main_queue(), ^{
              [card setImage:greyImage];
            });
          });
        } else {
          [card setImage:image];
        }
      });
}

// Move cards according to |currentPoint_.x|. Edge cards only drag
// |kEdgeCardDragPercentage| of |bounds|.
- (void)updateCardPositions {
  CGFloat width = [self cardWidth];

  if ([self isEdgeSwipe]) {
    // If an edge card, don't allow the card to be dragged across the screen.
    // Instead, drag across |kEdgeCardDragPercentage| of the screen.
    _rightCard.transform = CGAffineTransformMakeTranslation(
        (currentPoint_.x) * kEdgeCardDragPercentage, 0);
    _leftCard.transform = CGAffineTransformMakeTranslation(
        (currentPoint_.x - width) * kEdgeCardDragPercentage, 0);
  } else {
    CGFloat rightXBuffer = kCardHorizontalSpacing * currentPoint_.x / width;
    CGFloat leftXBuffer = kCardHorizontalSpacing - rightXBuffer;

    _rightCard.transform =
        CGAffineTransformMakeTranslation(currentPoint_.x + rightXBuffer, 0);
    _leftCard.transform = CGAffineTransformMakeTranslation(
        -width + currentPoint_.x - leftXBuffer, 0);
  }
}

// Update layout with new touch event.
- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture {
  currentPoint_ = [gesture locationInView:self];
  currentPoint_.x -= gesture.swipeOffset;

  // Since it's difficult to touch the very edge of the screen (touches tend to
  // sit near x ~ 4), push the touch towards the edge.
  CGFloat width = [self cardWidth];
  CGFloat half = floor(width / 2);
  CGFloat padding = floor(std::abs(currentPoint_.x - half) / half);

  // Push towards the edges.
  if (currentPoint_.x > half)
    currentPoint_.x += padding;
  else
    currentPoint_.x -= padding;

  // But don't go past the edges.
  if (currentPoint_.x < 0)
    currentPoint_.x = 0;
  else if (currentPoint_.x > width)
    currentPoint_.x = width;

  [self updateCardPositions];

  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateCancelled ||
      gesture.state == UIGestureRecognizerStateFailed) {
    [self finishPan];
  }
}

- (BOOL)isEdgeSwipe {
  int currentIndex = model_.webStateList->active_index();
  return (IsSwipingBack(_direction) && currentIndex == 0) ||
         (IsSwipingForward(_direction) &&
          currentIndex == model_.webStateList->count() - 1);
}

// Update the current WebState and animate the proper card view if the
// |currentPoint_| is past the center of |bounds|.
- (void)finishPan {
  WebStateList* webStateList = model_.webStateList;
  int currentIndex = webStateList->active_index();
  // Something happened and now there is not active WebState.  End card side let
  // swipe and BVC show no tabs UI.
  if (currentIndex == WebStateList::kInvalidIndex)
    return [_delegate sideSwipeViewDismissAnimationDidEnd:self];

  CGFloat width = [self cardWidth];
  CGAffineTransform rightTransform, leftTransform;
  SwipeView* dominantCard;
  int destinationWebStateIndex = currentIndex;
  CGFloat offset = UseRTLLayout() ? -1 : 1;
  if (_direction == UISwipeGestureRecognizerDirectionRight) {
    // If swipe is right and |currentPoint_.x| is over the first 1/3, move left.
    if (currentPoint_.x > width / 3.0 && ![self isEdgeSwipe]) {
      rightTransform =
          CGAffineTransformMakeTranslation(width + kCardHorizontalSpacing, 0);
      leftTransform = CGAffineTransformIdentity;
      destinationWebStateIndex = currentIndex - offset;
      dominantCard = _leftCard;
      base::RecordAction(UserMetricsAction("MobileStackSwipeCompleted"));
    } else {
      leftTransform =
          CGAffineTransformMakeTranslation(-width - kCardHorizontalSpacing, 0);
      rightTransform = CGAffineTransformIdentity;
      dominantCard = _rightCard;
      base::RecordAction(UserMetricsAction("MobileStackSwipeCancelled"));
    }
  } else {
    // If swipe is left and |currentPoint_.x| is over the first 1/3, move right.
    if (currentPoint_.x < (width / 3.0) * 2.0 && ![self isEdgeSwipe]) {
      leftTransform =
          CGAffineTransformMakeTranslation(-width - kCardHorizontalSpacing, 0);
      rightTransform = CGAffineTransformIdentity;
      destinationWebStateIndex = currentIndex + offset;
      dominantCard = _rightCard;
      base::RecordAction(UserMetricsAction("MobileStackSwipeCompleted"));
    } else {
      rightTransform =
          CGAffineTransformMakeTranslation(width + kCardHorizontalSpacing, 0);
      leftTransform = CGAffineTransformIdentity;
      dominantCard = _leftCard;
      base::RecordAction(UserMetricsAction("MobileStackSwipeCancelled"));
    }
  }

  if (destinationWebStateIndex != currentIndex) {
    // The old webstate is now hidden. The new WebState will be inserted once
    // the animation is complete.
    webStateList->GetActiveWebState()->WasHidden();
  }

  // Make sure the dominant card animates on top.
  [dominantCard.superview bringSubviewToFront:dominantCard];

  [UIView animateWithDuration:kAnimationDuration
      animations:^{
        _leftCard.transform = leftTransform;
        _rightCard.transform = rightTransform;
      }
      completion:^(BOOL finished) {
        [_leftCard setImage:nil];
        [_rightCard setImage:nil];
        [_leftCard setTopToolbarImage:nil];
        [_rightCard setTopToolbarImage:nil];
        [_leftCard setBottomToolbarImage:nil];
        [_rightCard setBottomToolbarImage:nil];
        [_delegate sideSwipeViewDismissAnimationDidEnd:self];
        // Changing the model even when the webstate is the same at the end of
        // the animation allows the UI to recover.  This call must come last,
        // because ActivateWebStateAt triggers behavior that depends on the view
        // hierarchy being reassembled, which happens in
        // sideSwipeViewDismissAnimationDidEnd.
        webStateList->ActivateWebStateAt(destinationWebStateIndex);
      }];
}

@end
