// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_to_grid_animation.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_parameters.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation TabToGridAnimation {
  // The animation parameters for the current animation.
  TabGridAnimationParameters* _animationParameters;
}

- (instancetype)initWithAnimationParameters:
    (TabGridAnimationParameters*)animationParameters {
  self = [super init];
  if (self) {
    _animationParameters = animationParameters;
  }
  return self;
}

- (void)animateWithCompletion:(ProceduralBlock)completion {
  UIView* animatedView = _animationParameters.animatedView;
  CGRect destinationFrame = _animationParameters.destinationFrame;
  CGRect originFrame = _animationParameters.originFrame;
  UIImage* contentSnapshot = _animationParameters.contentSnapshot;
  CGFloat topToolbarHeight = _animationParameters.topToolbarHeight;
  UIView* topToolbarSnapshotView = _animationParameters.topToolbarSnapshotView;
  UIView* bottomToolbarSnapshotView =
      _animationParameters.bottomToolbarSnapshotView;
  BOOL isIncognito = _animationParameters.incognito;
  UIView* activeGridView = _animationParameters.activeGrid.view;
  UIView* pinnedTabsView = _animationParameters.pinnedTabs.view;
  BOOL isActiveCellPinned = _animationParameters.activeCellPinned;
  BOOL isTopToolbarHidden = _animationParameters.topToolbarHidden;

  // Ratio of destination frame width over the current frame width.
  CGFloat destinationOverCurrentFrameRatio =
      destinationFrame.size.width / animatedView.frame.size.width;

  // Create top toolbar background view, add the snapshot to it and insert them
  // into the animated view, correctly positioned.
  UIView* topToolbarBackground = CreateToolbarSnapshotBackgroundView(
      topToolbarSnapshotView, animatedView, isIncognito, NO, CGRectZero);

  // Create bottom toolbar background view, add the snapshot to it and insert
  // them into the animated view, correctly positioned.
  UIView* bottomToolbarBackground = CreateToolbarSnapshotBackgroundView(
      bottomToolbarSnapshotView, animatedView, isIncognito, YES, originFrame);

  // Add the content snapshot to animated view.
  TopAlignedImageView* contentImageView = [[TopAlignedImageView alloc] init];
  contentImageView.image = contentSnapshot;
  contentImageView.alpha = 1;
  [animatedView addSubview:contentImageView];

  // Create and set the content snapshot's frame.
  CGRect imageViewOriginFrame = CGRectMake(
      0, topToolbarHeight, originFrame.size.width, contentSnapshot.size.height);
  contentImageView.frame = imageViewOriginFrame;

  // Create the content snapshot's destination frame.
  CGFloat destinationFrameAspectRatio =
      destinationFrame.size.width / destinationFrame.size.height;
  CGRect imageViewDestinationFrame =
      CGRectMake(0, topToolbarHeight, originFrame.size.width,
                 originFrame.size.width / destinationFrameAspectRatio);

  // Needed so that the contentImageView's innerImageView frame is not
  // CGRectZero when the animation starts.
  [contentImageView setNeedsLayout];
  [contentImageView layoutIfNeeded];

  // Create a rounded rectangle mask which will be used to the hide the toolbars
  // and apply the new aspect ratio during the animation.
  int gridCellCornerRadius =
      kGridCellCornerRadius / destinationOverCurrentFrameRatio;
  CGFloat bottomCrop = ((animatedView.frame.size.height - topToolbarHeight) *
                        destinationOverCurrentFrameRatio) -
                       destinationFrame.size.height;

  UIBezierPath* croppedToolbarPath =
      CreateTabGridAnimationRoundedRectPathWithInsets(
          animatedView.frame, bottomCrop / destinationOverCurrentFrameRatio,
          topToolbarHeight, gridCellCornerRadius);

  // Create new path that follows device corner radius, and doesn't crop the
  // toolbars.
  UIBezierPath* noCropPath =
      [UIBezierPath bezierPathWithRoundedRect:animatedView.bounds
                                 cornerRadius:DeviceCornerRadius()];

  // Set the mask on the animated view.
  CAShapeLayer* mask = CreateTabGridAnimationMaskWithFrame(animatedView.frame);
  mask.path = croppedToolbarPath.CGPath;
  animatedView.layer.mask = mask;

  // Animate the mask "addition", and from the device corner radius to the grid
  // cell corner radius.
  CABasicAnimation* animation = [CABasicAnimation animationWithKeyPath:@"path"];
  animation.fromValue = (id)(noCropPath.CGPath);
  animation.toValue = (id)(croppedToolbarPath.CGPath);
  animation.duration =
      kTabToGridAnimationDuration * (isActiveCellPinned ? 0.6 : 0.7);
  animation.timingFunction = [CAMediaTimingFunction
      functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
  [animatedView.layer.mask addAnimation:animation forKey:@"maskAnimation"];

  // Active tab grid blur animation setup.
  UIVisualEffectView* activeGridBlurView = [[UIVisualEffectView alloc]
      initWithEffect:[UIBlurEffect effectWithStyle:kTabGridBlurStyle]];
  activeGridBlurView.translatesAutoresizingMaskIntoConstraints = NO;
  [animatedView.superview insertSubview:activeGridBlurView
                           belowSubview:animatedView];
  AddSameConstraints(activeGridBlurView.superview, activeGridBlurView);

  [animatedView.superview setNeedsLayout];
  [animatedView.superview layoutIfNeeded];

  // Active grid zoom animation setup.
  [activeGridView setNeedsLayout];
  [activeGridView layoutIfNeeded];

  // Find the center of the destination frame in the grid view's coordinate
  // system. This is needed because of the grid view's scroll view.
  SetAnchorPointToFrameCenter(activeGridView, destinationFrame);
  SetAnchorPointToFrameCenter(pinnedTabsView, destinationFrame);

  // Scale the grid view before the animation.
  activeGridView.transform = CGAffineTransformMakeScale(kTabGridAnimationScale,
                                                        kTabGridAnimationScale);
  pinnedTabsView.transform = CGAffineTransformMakeScale(kTabGridAnimationScale,
                                                        kTabGridAnimationScale);

  // The main tab grid transition animation.
  void (^mainAnimation)() = ^{
    // Animate the removal of the blur and zooming of the grid view.
    activeGridBlurView.effect = nil;
    activeGridView.transform = CGAffineTransformIdentity;
    pinnedTabsView.transform = CGAffineTransformIdentity;

    // Needed so that the contentImageView's innerImageView frame is
    // animated.
    contentImageView.frame = imageViewDestinationFrame;
    [contentImageView setNeedsLayout];
    [contentImageView layoutIfNeeded];

    // Scale animated view to destination frame.
    animatedView.transform = CGAffineTransformMakeScale(
        destinationOverCurrentFrameRatio, destinationOverCurrentFrameRatio);

    // Compensate for mask height difference, translate scaled animated view to
    // be top-aligned with the originFrame.
    CGRect newFrame = animatedView.frame;
    newFrame.origin = destinationFrame.origin;
    newFrame.origin.y -= topToolbarHeight * destinationOverCurrentFrameRatio;
    animatedView.frame = newFrame;
  };

  // The main animation's completion block.
  void (^mainCompletion)(BOOL) = ^(BOOL finished) {
    // Reset the active grid view.
    CGRect oldAnimationFrame = activeGridView.frame;
    activeGridView.layer.anchorPoint = CGPointMake(0.5, 0.5);
    activeGridView.frame = oldAnimationFrame;

    // Reset the pinned tabs view.
    CGRect oldPinnedTabsFrame = pinnedTabsView.frame;
    pinnedTabsView.layer.anchorPoint = CGPointMake(0.5, 0.5);
    pinnedTabsView.frame = oldPinnedTabsFrame;

    // Cleanup.
    animatedView.transform = CGAffineTransformIdentity;
    animatedView.layer.mask = nil;
    [activeGridBlurView removeFromSuperview];
    [topToolbarBackground removeFromSuperview];
    [bottomToolbarBackground removeFromSuperview];
    [contentImageView removeFromSuperview];

    if (completion) {
      completion();
    }
  };

  // Toolbars animation.
  void (^toolbarsAnimation)() = ^{
    if (!isTopToolbarHidden) {
      // If the top toolbar is hidden, the snapshot should not be fade out as
      // the toolbar background behind it should not appear.
      topToolbarSnapshotView.alpha = 0;
    }
    bottomToolbarSnapshotView.alpha = 0;
  };

  // Fade out the animated view towards the end of the animation, only
  // applicable to a pinned tab.
  void (^fadeOutPinnedTabCellAnimation)() = ^{
    animatedView.alpha = 0;
  };

  // Animate the toolbars with a relative duration.
  void (^relativeStartAndDurationToolbarsAnimation)() = ^{
    [UIView addKeyframeWithRelativeStartTime:0
                            relativeDuration:
                                kTabToGridToolbarsAnimationRelativeStartTime
                                  animations:toolbarsAnimation];
  };

  if (isActiveCellPinned) {
    [UIView animateWithDuration:kTabToGridAnimationDuration / 2
                          delay:kTabToGridAnimationDuration / 2
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:fadeOutPinnedTabCellAnimation
                     completion:^(BOOL) {
                       animatedView.alpha = 1.0;
                     }];
  }

  // Perform the toolbars animation.
  [UIView animateKeyframesWithDuration:kTabToGridAnimationDuration
                                 delay:0
                               options:UIViewAnimationOptionCurveLinear
                            animations:relativeStartAndDurationToolbarsAnimation
                            completion:nil];

  // Perform the main animation.
  [UIView animateWithDuration:kTabToGridAnimationDuration
                        delay:0
       usingSpringWithDamping:kTabToGridAnimationDamping
        initialSpringVelocity:kTabGridTransitionAnimationInitialVelocity
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:mainAnimation
                   completion:mainCompletion];
}

@end
