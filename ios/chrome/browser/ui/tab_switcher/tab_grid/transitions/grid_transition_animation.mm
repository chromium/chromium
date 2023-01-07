// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_animation.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_to_tab_transition_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/util/property_animator_group.h"
#import "ios/chrome/common/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Scale factor for inactive items when a tab is expanded.
const CGFloat kInactiveItemScale = 0.95;

}

@interface GridTransitionAnimation ()
// The property animator group backing the public `animator` property.
@property(nonatomic, readonly) PropertyAnimatorGroup* animations;
// The layout of the grid for this animation.
@property(nonatomic, strong) GridTransitionLayout* layout;
// The duration of the animation.
@property(nonatomic, readonly, assign) NSTimeInterval duration;
// The direction this animation is in.
@property(nonatomic, readonly, assign) GridAnimationDirection direction;
// Corner radius that the active cell will have when it is animated into the
// regulat grid.
@property(nonatomic, assign) CGFloat finalActiveCellCornerRadius;
@end

@implementation GridTransitionAnimation

@synthesize activeCell = _activeCell;

@synthesize animations = _animations;
@synthesize layout = _layout;
@synthesize duration = _duration;
@synthesize direction = _direction;
@synthesize finalActiveCellCornerRadius = _finalActiveCellCornerRadius;

- (instancetype)initWithLayout:(GridTransitionLayout*)layout
                      duration:(NSTimeInterval)duration
                     direction:(GridAnimationDirection)direction {
  if (self = [super initWithFrame:CGRectZero]) {
    _animations = [[PropertyAnimatorGroup alloc] init];
    _layout = layout;
    _duration = duration;
    _direction = direction;
    _finalActiveCellCornerRadius = _layout.activeItem.cell.cornerRadius;
  }
  return self;
}

- (id<UIViewImplicitlyAnimating>)animator {
  return self.animations;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  self.frame = newSuperview.bounds;
  if (newSuperview && self.subviews.count == 0) {
    [self prepareForAnimationInSuperview:newSuperview];
  }
}

- (void)didMoveToSuperview {
  if (!self.superview)
    return;
  // Positioning the animating items depends on converting points to this
  // view's coordinate system, so wait until it's in a view hierarchy.
  switch (self.direction) {
    case GridAnimationDirectionContracting:
      [self positionExpandedActiveItem];
      [self prepareInactiveItemsForAppearance];
      [self buildContractingAnimations];
      break;
    case GridAnimationDirectionExpanding:
      [self prepareAllItemsForExpansion];
      [self buildExpandingAnimations];
      break;
  }
  // Make sure all of the layout after the view setup is complete before any
  // animations are run.
  [self layoutIfNeeded];
}

#pragma mark - Private methods

- (void)buildContractingAnimations {
  // The transition is structured as three or five separate animations. They are
  // timed based on various sub-durations and delays which are expressed as
  // fractions of the overall animation duration.
  CGFloat partialDuration = 0.6;
  CGFloat briefDuration = partialDuration * 0.5;
  CGFloat shortDelay = 0.2;

  // Damping ratio for the resize animation.
  CGFloat resizeDamping = 0.8;

  // If there's only one cell, the animation has two parts.
  //   (A) Zooming the active cell into position.
  //   (B) Crossfading from the tab to cell top view.
  //   (C) Rounding the corners of the active cell.
  //
  //  {0%}----------------------[A]-------------------{100%}
  //                            {50%}----[B]----{80%}
  //  {0%}---[C]---{30%}

  // If there's more than once cell, the animation adds two more parts:
  //   (D) Scaling up the inactive cells.
  //   (E) Fading the inactive cells to 100% opacity.
  // The overall timing is as follows:
  //
  //  {0%}----------------------[A]-------------------{100%}
  //                            {50%}----[B]----{80%}
  //  {0%}---[C]---{30%}
  //           {20%}--[D]-----------------------------{100%}
  //           {20%}--[E]-----------------------{80%}
  //
  // (Changing the timing constants above will change the timing % values)

  UIView<GridToTabTransitionView>* activeCell = self.layout.activeItem.cell;
  // The final cell snapshot exactly matches the main tab view of the cell, so
  // it can have an alpha of 0 for the whole animation.
  activeCell.mainTabView.alpha = 0.0;
  // The final cell header starts at 0 alpha and is cross-faded in.
  activeCell.topCellView.alpha = 0.0;

  // A: Zoom the active cell into position.
  auto zoomActiveCellAnimation = ^{
    [self positionAndScaleActiveItemInGrid];
  };

  UIViewPropertyAnimator* zoomActiveCell =
      [[UIViewPropertyAnimator alloc] initWithDuration:self.duration
                                          dampingRatio:resizeDamping
                                            animations:zoomActiveCellAnimation];
  [self.animations addAnimator:zoomActiveCell];

  // B: Fade in the active cell top cell view, fade out the active cell's
  // top tab view.
  auto fadeInAuxillaryKeyframeAnimation =
      [self keyframeAnimationFadingView:activeCell.topTabView
                          throughToView:activeCell.topCellView
                          relativeStart:0.5
                       relativeDuration:briefDuration];

  UIViewPropertyAnimator* fadeInAuxillary = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseInOut
            animations:fadeInAuxillaryKeyframeAnimation];
  [self.animations addAnimator:fadeInAuxillary];

  // C: Round the corners of the active cell.
  UIView<GridToTabTransitionView>* cell = self.layout.activeItem.cell;
  cell.cornerRadius = DeviceCornerRadius();
  auto roundCornersAnimation = ^{
    cell.cornerRadius = self.finalActiveCellCornerRadius;
  };
  auto roundCornersKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:0
                              relativeDuration:briefDuration
                                    animations:roundCornersAnimation];
  UIViewPropertyAnimator* roundCorners = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveLinear
            animations:roundCornersKeyframeAnimation];
  [self.animations addAnimator:roundCorners];

  // Single cell case.
  if (self.layout.inactiveItems.count == 0)
    return;

  // Additional animations for multiple cells.
  // D: Scale up inactive cells.
  auto scaleUpCellsAnimation = ^{
    for (GridTransitionItem* item in self.layout.inactiveItems) {
      item.cell.transform = CGAffineTransformIdentity;
    }
  };

  auto scaleUpCellsKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:shortDelay
                              relativeDuration:1 - shortDelay
                                    animations:scaleUpCellsAnimation];
  UIViewPropertyAnimator* scaleUpCells = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseOut
            animations:scaleUpCellsKeyframeAnimation];
  [self.animations addAnimator:scaleUpCells];

  // E: Fade in inactive cells.
  auto fadeInCellsAnimation = ^{
    for (GridTransitionItem* item in self.layout.inactiveItems) {
      item.cell.alpha = 1.0;
    }
  };
  auto fadeInCellsKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:shortDelay
                              relativeDuration:partialDuration
                                    animations:fadeInCellsAnimation];
  UIViewPropertyAnimator* fadeInCells = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseOut
            animations:fadeInCellsKeyframeAnimation];
  [self.animations addAnimator:fadeInCells];
}

- (void)buildExpandingAnimations {
  // The transition is structured as four to six separate animations. They are
  // timed based on two sub-durations which are expressed as fractions of the
  // overall animation duration.
  CGFloat partialDuration = 0.66;
  CGFloat briefDuration = 0.3;
  CGFloat delay = 0.1;

  // Damping ratio for the resize animation.
  CGFloat resizeDamping = 0.7;

  // If there's only one cell, the animation has three parts:
  //   (A) Zooming the active cell out into the expanded position.
  //   (B) Crossfading the active cell's top views.
  //   (C) Squaring the corners of the active cell.
  //   (D) Fading out the main cell view and fading in the main tab view, if
  //       necessary.
  // These parts are timed over `duration` like this:
  //
  //  {0%}--[A]-----------------------------------{100%}
  //  {0%}--[B]---{30%}
  //  {0%}--[C]---{30%}
  //    {10%}--[D]---{40%}

  // If there's more than once cell, the animation adds:
  //   (E) Scaling the inactive cells to 95%
  //   (F) Fading out the inactive cells.
  // The overall timing is as follows:
  //
  //  {0%}--[A]-----------------------------------{100%}
  //  {0%}--[B]---{30%}
  //  {0%}--[C]---{30%}
  //    {10%}--[D]---{40%}
  //  {0%}--[E]-----------------------------------{100%}
  //  {0%}--[F]-------------------{66%}
  //
  // All animations are timed ease-out (so more motion happens sooner), except
  // for B, C and D. B is a crossfade and eases in/out. C and D are relatively
  // short in duration; they have linear timing so they doesn't seem
  // instantaneous, and D is also linear so that identical views animate
  // smoothly.
  //
  // Animation D is necessary because the cell content and the tab content may
  // no longer match in aspect ratio; a quick cross-fade in mid-transition
  // prevents an abrupt jump when the transition ends and the "real" tab content
  // is shown.

  UIView<GridToTabTransitionView>* activeCell = self.layout.activeItem.cell;
  // The top tab view starts at zero alpha but is crossfaded in.
  activeCell.topTabView.alpha = 0.0;
  // If the active item is appearing, the main tab view is shown. If not, it's
  // hidden, and may be faded in if it's expected to be different in content
  // from the existing cell snapshot.
  if (!self.layout.activeItem.isAppearing)
    activeCell.mainTabView.alpha = 0.0;

  // A: Zoom the active cell into position.
  UIViewPropertyAnimator* zoomActiveCell =
      [[UIViewPropertyAnimator alloc] initWithDuration:self.duration
                                          dampingRatio:resizeDamping
                                            animations:^{
                                              [self positionExpandedActiveItem];
                                            }];
  [self.animations addAnimator:zoomActiveCell];

  // B: Crossfade the top views.
  auto fadeOutAuxilliaryAnimation =
      [self keyframeAnimationWithRelativeStart:0
                              relativeDuration:briefDuration
                                    animations:^{
                                      activeCell.topCellView.alpha = 0;
                                      activeCell.topTabView.alpha = 1.0;
                                    }];
  UIViewPropertyAnimator* fadeOutAuxilliary = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseInOut
            animations:fadeOutAuxilliaryAnimation];
  [self.animations addAnimator:fadeOutAuxilliary];

  // C: Square the active cell's corners.
  UIView<GridToTabTransitionView>* cell = self.layout.activeItem.cell;
  auto squareCornersAnimation = ^{
    cell.cornerRadius = DeviceCornerRadius();
  };
  auto squareCornersKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:0.0
                              relativeDuration:briefDuration
                                    animations:squareCornersAnimation];
  UIViewPropertyAnimator* squareCorners = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveLinear
            animations:squareCornersKeyframeAnimation];
  [self.animations addAnimator:squareCorners];

  // D: crossfade the main cell content, if necessary.
  // This crossfade is needed if the aspect ratio of the tab being animated
  // to doesn't match the aspect ratio of the tab that originally generated the
  // cell content being animated; this happens when the tab grid is exited in a
  // diffferent orientation than it was entered.
  // Using a linear animation curve means that the sum of the opacities is
  // contstant though the animation, which will help it seem less abrupt by
  // keeping a relatively constant brightness.
  if (self.layout.frameChanged) {
    auto crossfadeContentAnimation =
        [self keyframeAnimationWithRelativeStart:delay
                                relativeDuration:briefDuration
                                      animations:^{
                                        activeCell.mainCellView.alpha = 0;
                                        activeCell.mainTabView.alpha = 1.0;
                                      }];
    UIViewPropertyAnimator* crossfadeContent = [[UIViewPropertyAnimator alloc]
        initWithDuration:self.duration
                   curve:UIViewAnimationCurveLinear
              animations:crossfadeContentAnimation];
    [self.animations addAnimator:crossfadeContent];
  }
  // If there's only a single cell, that's all.
  if (self.layout.inactiveItems.count == 0)
    return;

  // Additional animations for multiple cells.
  // E: Scale down inactive cells.
  auto scaleDownCellsAnimation = ^{
    for (GridTransitionItem* item in self.layout.inactiveItems) {
      item.cell.transform = CGAffineTransformScale(
          item.cell.transform, kInactiveItemScale, kInactiveItemScale);
    }
  };
  UIViewPropertyAnimator* scaleDownCells = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseOut
            animations:scaleDownCellsAnimation];
  [self.animations addAnimator:scaleDownCells];

  // F: Fade out inactive cells.
  auto fadeOutCellsAnimation = ^{
    for (GridTransitionItem* item in self.layout.inactiveItems) {
      item.cell.alpha = 0.0;
    }
  };
  auto fadeOutCellsKeyframeAnimation =
      [self keyframeAnimationWithRelativeStart:0
                              relativeDuration:partialDuration
                                    animations:fadeOutCellsAnimation];
  UIViewPropertyAnimator* fadeOutCells = [[UIViewPropertyAnimator alloc]
      initWithDuration:self.duration
                 curve:UIViewAnimationCurveEaseOut
            animations:fadeOutCellsKeyframeAnimation];
  [self.animations addAnimator:fadeOutCells];
}

// Performs the initial setup for the animation, computing scale based on the
// superview size and adding the transition cells to the view hierarchy.
- (void)prepareForAnimationInSuperview:(UIView*)newSuperview {
  // Add the selection item first, so it's under ther other views.
  [self addSubview:self.layout.selectionItem.cell];

  for (GridTransitionItem* item in self.layout.inactiveItems) {
    [self addSubview:item.cell];
  }

  // Add the active item last so it's always the top subview.
  [self addSubview:self.layout.activeItem.cell];
}

// Positions the active item in the expanded grid position with a zero corner
// radius and a 0% opacity auxilliary view.
- (void)positionExpandedActiveItem {
  UIView<GridToTabTransitionView>* cell = self.layout.activeItem.cell;
  cell.frame = self.layout.expandedRect;
  [cell positionTabViews];
}

// Positions all of the inactive items in their grid positions.
// Fades and scales each of those items.
- (void)prepareInactiveItemsForAppearance {
  for (GridTransitionItem* item in self.layout.inactiveItems) {
    [self positionItemInGrid:item];
    item.cell.alpha = 0.2;
    item.cell.transform = CGAffineTransformScale(
        item.cell.transform, kInactiveItemScale, kInactiveItemScale);
  }
  [self positionItemInGrid:self.layout.selectionItem];
}

// Positions the active item in the regular grid position with its final
// corner radius.
- (void)positionAndScaleActiveItemInGrid {
  UIView<GridToTabTransitionView>* cell = self.layout.activeItem.cell;
  cell.transform = CGAffineTransformIdentity;
  CGRect frame = cell.frame;
  frame.size = self.layout.activeItem.size;
  cell.frame = frame;
  [self positionItemInGrid:self.layout.activeItem];
  [cell positionCellViews];
}

// Prepares all of the items for an expansion anumation.
- (void)prepareAllItemsForExpansion {
  for (GridTransitionItem* item in self.layout.inactiveItems) {
    [self positionItemInGrid:item];
  }
  [self positionItemInGrid:self.layout.activeItem];
  [self.layout.activeItem.cell positionCellViews];
  self.activeCell = self.layout.activeItem.cell;
  [self positionItemInGrid:self.layout.selectionItem];
}

// Positions `item` in it grid position.
- (void)positionItemInGrid:(GridTransitionItem*)item {
  UIView* cell = item.cell;
  CGPoint newCenter = [self.superview convertPoint:item.center fromView:nil];
  cell.center = newCenter;
}

// Helper function to construct keyframe animation blocks.
// Given `start` and `duration` (in the [0.0-1.0] interval), returns an
// animation block which runs `animations` starting at `start` (relative to
// `self.duration`) and running for `duration` (likewise).
- (void (^)(void))keyframeAnimationWithRelativeStart:(double)start
                                    relativeDuration:(double)duration
                                          animations:
                                              (void (^)(void))animations {
  auto keyframe = ^{
    [UIView addKeyframeWithRelativeStartTime:start
                            relativeDuration:duration
                                  animations:animations];
  };
  return ^{
    [UIView animateKeyframesWithDuration:self.duration
                                   delay:0
                                 options:UIViewAnimationOptionLayoutSubviews
                              animations:keyframe
                              completion:nil];
  };
}

// Returns a cross-fade keyframe animation between two views.
// `startView` should have an alpha of 1; `endView` should have an alpha of 0.
// `start` and `duration` are in the [0.0]-[1.0] interval and represent timing
// relative to `self.duration`.
// The animation returned by this method will fade `startView` to 0 over the
// first half of `duration`, and then fade `endView` to 1.0 over the second
// half, preventing any blurred frames showing both views. For best results, the
// animation curev should be EaseInEaseOut.
- (void (^)(void))keyframeAnimationFadingView:(UIView*)startView
                                throughToView:(UIView*)endView
                                relativeStart:(double)start
                             relativeDuration:(double)duration {
  CGFloat halfDuration = duration / 2;
  auto keyframes = ^{
    [UIView addKeyframeWithRelativeStartTime:start
                            relativeDuration:halfDuration
                                  animations:^{
                                    startView.alpha = 0.0;
                                  }];
    [UIView addKeyframeWithRelativeStartTime:start + halfDuration
                            relativeDuration:halfDuration
                                  animations:^{
                                    endView.alpha = 1.0;
                                  }];
  };
  return ^{
    [UIView animateKeyframesWithDuration:self.duration
                                   delay:0
                                 options:UIViewAnimationOptionLayoutSubviews
                              animations:keyframes
                              completion:nil];
  };
}

@end
