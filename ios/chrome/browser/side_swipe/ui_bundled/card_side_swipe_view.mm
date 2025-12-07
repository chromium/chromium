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
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/card_swipe_view_delegate.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_util.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/swipe_view.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/side_swipe_toolbar_snapshot_providing.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_type.h"
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

  // Used to fetch snapshot for tabs.
  raw_ptr<SnapshotBrowserAgent> _snapshotBrowserAgent;
}

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame
                    topMargin:(CGFloat)topMargin
                 webStateList:(WebStateList*)webStateList
         snapshotBrowserAgent:(SnapshotBrowserAgent*)snapshotBrowserAgent {
  self = [super initWithFrame:frame];
  if (self) {
    _webStateList = webStateList;
    _snapshotBrowserAgent = snapshotBrowserAgent;
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

    _rightCard = [[SwipeView alloc] initWithFrame:CGRectZero
                                        topMargin:topMargin];
    _rightCard.layer.cornerRadius = kCardCornerRadius;
    _rightCard.layer.masksToBounds = YES;
    _leftCard = [[SwipeView alloc] initWithFrame:CGRectZero
                                       topMargin:topMargin];
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
  if (_currentPoint.x > half) {
    _currentPoint.x += padding;
  } else {
    _currentPoint.x -= padding;
  }

  // But don't go past the edges.
  if (_currentPoint.x < 0) {
    _currentPoint.x = 0;
  } else if (_currentPoint.x > width) {
    _currentPoint.x = width;
  }

  [self updateCardPositions];

  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateCancelled ||
      gesture.state == UIGestureRecognizerStateFailed) {
    [self finishPanWithActionBeforeTabSwitch:actionBeforeTabSwitch];
  }
}

- (void)disconnect {
  _snapshotBrowserAgent = nullptr;
  _webStateList = nullptr;
}

#pragma mark - Properties

- (void)setTopMargin:(CGFloat)topMargin {
  _topMargin = topMargin;
  _leftCard.topMargin = topMargin;
  _rightCard.topMargin = topMargin;
}

#pragma mark - UIView

- (void)updateConstraints {
  [super updateConstraints];
  CGFloat topInset = self.safeAreaInsets.top;
  _backgroundTopConstraint.constant = -topInset;
}

#pragma mark - Private

- (CGFloat)cardWidth {
  return CGRectGetWidth(self.bounds);
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
  PrefService* prefs =
      ProfileIOS::FromBrowserState(webState->GetBrowserState())->GetPrefs();
  // Lens overlay displays content fullscreen and hides the vertical toolbars.
  if (IsLensOverlayAvailable(prefs)) {
    if (LensOverlayTabHelper* lensOverlayTabHelper =
            LensOverlayTabHelper::FromWebState(webState)) {
      BOOL lensOverlayShown;

      if (IsLensOverlaySameTabNavigationEnabled(prefs)) {
        lensOverlayShown =
            lensOverlayTabHelper->IsLensOverlayInvokedOnCurrentNavigationItem();
      } else {
        lensOverlayShown =
            lensOverlayTabHelper->IsLensOverlayUIAttachedAndAlive();
      }

      UIImage* lensOverlaySnapshot =
          lensOverlayTabHelper->GetViewportSnapshot();
      if (lensOverlayShown && lensOverlaySnapshot) {
        [self enableFullscreenCard:card];
        [card setImage:lensOverlaySnapshot];
        return;
      }
    }
  }

  UIImage* topToolbarSnapshot = [self.toolbarSnapshotProvider
      toolbarSideSwipeSnapshotForWebState:webState
                          withToolbarType:ToolbarType::kPrimary];
  [card setTopToolbarImage:topToolbarSnapshot];
  UIImage* bottomToolbarSnapshot = [self.toolbarSnapshotProvider
      toolbarSideSwipeSnapshotForWebState:webState
                          withToolbarType:ToolbarType::kSecondary];
  [card setBottomToolbarImage:bottomToolbarSnapshot];

  // Fetch grey scale image if the WebState is unrealized, or page placeholder
  // is requested for the next navigation, unless on single code devices (as
  // generating the greyscale image takes too much time on such slow device).
  SnapshotKind snapshotKind = SnapshotKindColor;
  if (!ios::device_util::IsSingleCoreDevice()) {
    if (!webState->IsRealized() ||
        PagePlaceholderTabHelper::FromWebState(webState)
            ->will_add_placeholder_for_next_navigation()) {
      snapshotKind = SnapshotKindGreyscale;
    }
  }

  __weak SwipeView* weakCard = card;
  _snapshotBrowserAgent->RetrieveSnapshotWithID(
      SnapshotID(webState->GetUniqueIdentifier()), snapshotKind,
      ^(UIImage* image) {
        [weakCard setImage:image];
      });
}

// Helper method that turns a card fullscreen by removing the vertical margins
// and the toolbar images.
- (void)enableFullscreenCard:(SwipeView*)card {
  [card setTopMargin:0];
  [card setTopToolbarImage:nil];
  [card setBottomToolbarImage:nil];
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
  if (currentIndex == WebStateList::kInvalidIndex) {
    return [self.delegate sideSwipeViewDismissAnimationDidEnd:self];
  }

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
        [weakSelf updateCardsWithLeftTransform:leftTransform
                                rightTransform:rightTransform];
      }
      completion:^(BOOL finished) {
        [weakSelf onAnimatePanComplete:destinationWebStateIndex];
      }];
}

// Updates the left/right cards with transforms.
- (void)updateCardsWithLeftTransform:(CGAffineTransform)leftCardTransform
                      rightTransform:(CGAffineTransform)rightCardTransform {
  _leftCard.transform = leftCardTransform;
  _rightCard.transform = rightCardTransform;
}

// Called when the pan animation is done, to handle cleanup.
- (void)onAnimatePanComplete:(int)destinationWebStateIndex {
  [_leftCard setImage:nil];
  [_rightCard setImage:nil];
  [_leftCard setTopToolbarImage:nil];
  [_rightCard setTopToolbarImage:nil];
  [_leftCard setBottomToolbarImage:nil];
  [_rightCard setBottomToolbarImage:nil];
  [self.delegate sideSwipeViewDismissAnimationDidEnd:self];
  // Changing the model even when the webstate is the same at the end of
  // the animation allows the UI to recover.  This call must come last,
  // because ActivateWebStateAt triggers behavior that depends on the view
  // hierarchy being reassembled, which happens in
  // sideSwipeViewDismissAnimationDidEnd.
  if (_webStateList && destinationWebStateIndex < _webStateList->count()) {
    // It seems possible that sometimes `destinationWebStateIndex` is bigger
    // than the last tab, probably because tabs were programmatically closed
    // during the swipe. See crbug.com/333961615.
    _webStateList->ActivateWebStateAt(destinationWebStateIndex);
  }
}

@end
