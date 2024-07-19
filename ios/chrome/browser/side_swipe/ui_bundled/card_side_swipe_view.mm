// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/card_side_swipe_view.h"

#import <cmath>

#import "base/ios/device_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_util.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/swipe_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/side_swipe_toolbar_snapshot_providing.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

using base::UserMetricsAction;

namespace {
// Spacing between cards.
const CGFloat kCardHorizontalSpacing = 16;

// Corner radius of cards.
const CGFloat kCardCornerRadius = 32;

// Portion of the screen an edge card can be dragged.
const CGFloat kEdgeCardDragPercentage = 0.35;

// Card animation times.
const NSTimeInterval kAnimationDuration = 0.15;

// Reduce size in -smallGreyImage by this factor.
const CGFloat kResizeFactor = 4;
}  // anonymous namespace

@implementation CardSideSwipeView {
  // Constraint defining the top edge of the background.
  NSLayoutConstraint* _backgroundTopConstraint;

  // The direction of the swipe that initiated this horizontal view.
  UISwipeGestureRecognizerDirection _direction;

  // Card views currently displayed.
  SwipeView* _leftCard;
  SwipeView* _rightCard;

  // Most recent touch location.
  CGPoint _currentPoint;

  // WebStateList provided from the initializer.
  raw_ptr<WebStateList> _webStateList;
}

@synthesize delegate = _delegate;
@synthesize topMargin = _topMargin;

- (instancetype)initWithFrame:(CGRect)frame
                    topMargin:(CGFloat)topMargin
                 webStateList:(WebStateList*)webStateList {
  self = [super initWithFrame:frame];
  if (self) {
    _webStateList = webStateList;
    _currentPoint = CGPointZero;
    _topMargin = topMargin;

    UIView* background = [[UIView alloc] initWithFrame:CGRectZero];
    [self addSubview:background];

    [background setTranslatesAutoresizingMaskIntoConstraints:NO];
    CGFloat topInset = self.safeAreaInsets.top;
    _backgroundTopConstraint =
        [[background topAnchor] constraintEqualToAnchor:self.topAnchor
                                               constant:-topInset];
    [NSLayoutConstraint activateConstraints:@[
      [[background rightAnchor] constraintEqualToAnchor:self.rightAnchor],
      [[background leftAnchor] constraintEqualToAnchor:self.leftAnchor],
      _backgroundTopConstraint,
      [[background bottomAnchor] constraintEqualToAnchor:self.bottomAnchor]
    ]];

    background.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];

    _rightCard =
        [[SwipeView alloc] initWithFrame:CGRectZero topMargin:topMargin];
    _rightCard.layer.cornerRadius = kCardCornerRadius;
    _rightCard.layer.masksToBounds = YES;
    _leftCard =
        [[SwipeView alloc] initWithFrame:CGRectZero topMargin:topMargin];
    _leftCard.layer.cornerRadius = kCardCornerRadius;
    _leftCard.layer.masksToBounds = YES;
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
  _backgroundTopConstraint.constant = -topInset;
}

- (CGFloat)cardWidth {
  return CGRectGetWidth(self.bounds);
}

// Set up left and right card views depending on current WebState and swipe
// direction.
- (void)updateViewsForDirection:(UISwipeGestureRecognizerDirection)direction {
  _direction = direction;
  int currentIndex = _webStateList->active_index();
  CGFloat offset = UseRTLLayout() ? -1 : 1;
  if (_direction == UISwipeGestureRecognizerDirectionRight) {
    [self setupCard:_rightCard withIndex:currentIndex];
    [self setupCard:_leftCard withIndex:currentIndex - offset];
  } else {
    [self setupCard:_leftCard withIndex:currentIndex];
    [self setupCard:_rightCard withIndex:currentIndex + offset];
  }
}

// Build a `kResizeFactor` sized greyscaled version of `image`.
- (UIImage*)smallGreyImage:(UIImage*)image {
  CGRect smallSize = CGRectMake(0, 0, image.size.width / kResizeFactor,
                                image.size.height / kResizeFactor);
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.opaque = YES;
  // Using CIFilter here on iOS 5+ might be faster, but it doesn't easily
  // allow for resizing.  At the max size, it's still too slow for side swipe.

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:smallSize.size
                                             format:format];

  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    UIBezierPath* background = [UIBezierPath bezierPathWithRect:smallSize];
    [UIColor.blackColor set];
    [background fill];

    [image drawInRect:smallSize blendMode:kCGBlendModeLuminosity alpha:1.0];
  }];
}

// Create card view based on `_webStateList`'s index.
- (void)setupCard:(SwipeView*)card withIndex:(int)index {
  if (index < 0 || index >= _webStateList->count()) {
    [card setHidden:YES];
    return;
  }
  [card setHidden:NO];

  web::WebState* webState = _webStateList->GetWebStateAt(index);
  UIImage* topToolbarSnapshot = [self.toolbarSnapshotProvider
      toolbarSideSwipeSnapshotForWebState:webState
                          withToolbarType:ToolbarType::kPrimary];
  [card setTopToolbarImage:topToolbarSnapshot];
  UIImage* bottomToolbarSnapshot = [self.toolbarSnapshotProvider
      toolbarSideSwipeSnapshotForWebState:webState
                          withToolbarType:ToolbarType::kSecondary];
  [card setBottomToolbarImage:bottomToolbarSnapshot];

  __weak CardSideSwipeView* weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();
  SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(^(
      UIImage* image) {
    [weakSelf colorSnapshotRetrieved:image card:card weakWebState:weakWebState];
  });
}

// Helper method that is invoked once the color snapshot has been fetched
// for the WebState returned by `weakWebState`. As the fetching is done
// asynchronously, it is possible for the WebState to have been destroyed
// and thus for `webStateGetter` to return nullptr.
- (void)colorSnapshotRetrieved:(UIImage*)image
                          card:(SwipeView*)card
                  weakWebState:(base::WeakPtr<web::WebState>)weakWebState {
  // If the WebState has been destroyed, the card will be dropped, so
  // the image can be dropped.
  web::WebState* webState = weakWebState.get();
  if (!webState) {
    return;
  }

  // Converting snapshotted images to grey takes too much time for single core
  // devices.  Instead, show the colored image for single core devices and the
  // grey image for multi core devices.
  const bool use_color_image =
      ios::device_util::IsSingleCoreDevice() ||
      !PagePlaceholderTabHelper::FromWebState(webState)
           ->will_add_placeholder_for_next_navigation();

  if (use_color_image) {
    [card setImage:image];
    return;
  }

  __weak CardSideSwipeView* weakSelf = self;
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0ul),
                 ^{
                   UIImage* greyImage = [weakSelf smallGreyImage:image];
                   if (greyImage) {
                     dispatch_async(dispatch_get_main_queue(), ^{
                       [card setImage:greyImage];
                     });
                   }
                 });
}

// Move cards according to `currentPoint_.x`. Edge cards only drag
// `kEdgeCardDragPercentage` of `bounds`.
- (void)updateCardPositions {
  CGFloat width = [self cardWidth];

  if ([self isEdgeSwipe]) {
    // If an edge card, don't allow the card to be dragged across the screen.
    // Instead, drag across `kEdgeCardDragPercentage` of the screen.
    _rightCard.transform = CGAffineTransformMakeTranslation(
        (_currentPoint.x) * kEdgeCardDragPercentage, 0);
    _leftCard.transform = CGAffineTransformMakeTranslation(
        (_currentPoint.x - width) * kEdgeCardDragPercentage, 0);
  } else {
    CGFloat rightXBuffer = kCardHorizontalSpacing * _currentPoint.x / width;
    CGFloat leftXBuffer = kCardHorizontalSpacing - rightXBuffer;

    _rightCard.transform =
        CGAffineTransformMakeTranslation(_currentPoint.x + rightXBuffer, 0);
    _leftCard.transform = CGAffineTransformMakeTranslation(
        -width + _currentPoint.x - leftXBuffer, 0);
  }
}

// Update layout with new touch event.
- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture
      actionBeforeTabSwitch:(TabSwipeHandler)actionBeforeTabSwitch {
  _currentPoint = [gesture locationInView:self];
  _currentPoint.x -= gesture.swipeOffset;

  // Since it's difficult to touch the very edge of the screen (touches tend to
  // sit near x ~ 4), push the touch towards the edge.
  CGFloat width = [self cardWidth];
  CGFloat half = floor(width / 2);
  CGFloat padding = floor(std::abs(_currentPoint.x - half) / half);

  // Push towards the edges.
  if (_currentPoint.x > half)
    _currentPoint.x += padding;
  else
    _currentPoint.x -= padding;

  // But don't go past the edges.
  if (_currentPoint.x < 0)
    _currentPoint.x = 0;
  else if (_currentPoint.x > width)
    _currentPoint.x = width;

  [self updateCardPositions];

  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateCancelled ||
      gesture.state == UIGestureRecognizerStateFailed) {
    [self finishPanWithActionBeforeTabSwitch:actionBeforeTabSwitch];
  }
}

// Returns whether the current card is an edge card based on swipe direction.
- (BOOL)isEdgeSwipe {
  int currentIndex = _webStateList->active_index();
  return (IsSwipingBack(_direction) && currentIndex == 0) ||
         (IsSwipingForward(_direction) &&
          currentIndex == _webStateList->count() - 1);
}

// Update the current WebState and animate the proper card view if the
// `currentPoint_` is past the center of `bounds`.
- (void)finishPanWithActionBeforeTabSwitch:
    (TabSwipeHandler)actionBeforeTabSwitch {
  int currentIndex = _webStateList->active_index();
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
    // If swipe is right and `currentPoint_.x` is over the first 1/3, move left.
    if (_currentPoint.x > width / 3.0 && ![self isEdgeSwipe]) {
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
    // If swipe is left and `currentPoint_.x` is over the first 1/3, move right.
    if (_currentPoint.x < (width / 3.0) * 2.0 && ![self isEdgeSwipe]) {
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

  if (actionBeforeTabSwitch) {
    actionBeforeTabSwitch(destinationWebStateIndex);
  }

  if (destinationWebStateIndex != currentIndex) {
    // The old webstate is now hidden. The new WebState will be inserted once
    // the animation is complete.
    _webStateList->GetActiveWebState()->WasHidden();
  }

  // Make sure the dominant card animates on top.
  [dominantCard.superview bringSubviewToFront:dominantCard];

  __weak CardSideSwipeView* weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration
      animations:^{
        [weakSelf animatePanWithLeftCardTransform:leftTransform
                               rightCardTransform:rightTransform];
      }
      completion:^(BOOL finished) {
        [weakSelf onAnimatePanComplete:destinationWebStateIndex];
      }];
}

- (void)animatePanWithLeftCardTransform:(CGAffineTransform)leftCardTransform
                     rightCardTransform:(CGAffineTransform)rightCardTransform {
  _leftCard.transform = leftCardTransform;
  _rightCard.transform = rightCardTransform;
}

- (void)onAnimatePanComplete:(int)destinationWebStateIndex {
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
  if (destinationWebStateIndex < _webStateList->count()) {
    // It seems possible that sometimes `destinationWebStateIndex` is bigger
    // than the last tab, probably because tabs were programmatically closed
    // during the swipe. See crbug.com/333961615.
    _webStateList->ActivateWebStateAt(destinationWebStateIndex);
  }
}

@end
