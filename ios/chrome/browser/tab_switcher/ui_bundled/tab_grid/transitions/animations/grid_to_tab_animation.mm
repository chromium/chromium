// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/grid_to_tab_animation.h"

#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_parameters.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation GridToTabAnimation {
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
  CGFloat topToolbarHeight = _animationParameters.topToolbarHeight;
  UIView* topToolbarSnapshotView = _animationParameters.topToolbarSnapshotView;
  UIView* bottomToolbarSnapshotView =
      _animationParameters.bottomToolbarSnapshotView;
  UIView* activeGridView = _animationParameters.activeGrid.view;
  UIView* pinnedTabsView = _animationParameters.pinnedTabs.view;
  BOOL isActiveCellPinned = _animationParameters.activeCellPinned;
  BOOL isIncognito = _animationParameters.incognito;

  // Ensure the browser view is "reset".
  animatedView.transform = CGAffineTransformIdentity;
  animatedView.frame = destinationFrame;

  // Ratio of the origin frame width over the destination frame width.
  CGFloat originOverDestinationFrameRatio =
      originFrame.size.width / destinationFrame.size.width;

  // Create top toolbar background view, add the snapshot to it and insert them
  // into the animated view, correctly positioned.
  UIView* topToolbarBackground = CreateToolbarSnapshotBackgroundView(
      topToolbarSnapshotView, animatedView, isIncognito, NO, CGRectZero);

  // Create bottom toolbar background view, add the snapshot to it and insert
  // them into the animated view, correctly positioned.
  UIView* bottomToolbarBackground = CreateToolbarSnapshotBackgroundView(
      bottomToolbarSnapshotView, animatedView, isIncognito, YES,
      destinationFrame);

  // Add the content snapshot to the animated view.
  TopAlignedImageView* contentImageView = [[TopAlignedImageView alloc] init];
  contentImageView.image = _animationParameters.contentSnapshot;
  contentImageView.alpha = 1;
  [animatedView addSubview:contentImageView];

  // Create and set the content snapshot's frame.
  CGFloat originFrameAspectRatio =
      originFrame.size.width / originFrame.size.height;
  CGRect imageViewOriginFrame =
      CGRectMake(0, topToolbarHeight, destinationFrame.size.width,
                 destinationFrame.size.width / originFrameAspectRatio);
  contentImageView.frame = imageViewOriginFrame;

  // Create the content snapshot's destination frame.
  CGRect imageViewDestinationFrame = CGRectMake(
      0, topToolbarHeight, destinationFrame.size.width,
      destinationFrame.size.height -
          (topToolbarHeight + _animationParameters.bottomToolbarHeight));

  // Needed so that the contentImageView's innerImageView frame is not
  // CGRectZero when the animation starts.
  [contentImageView setNeedsLayout];
  [contentImageView layoutIfNeeded];

  // Create a rounded rectangle mask which will be used to the hide the toolbars
  // and apply the new aspect ratio during the animation.
  int gridCellCornerRadius =
      kGridCellCornerRadius / originOverDestinationFrameRatio;
  CGFloat bottomCrop = ((destinationFrame.size.height - topToolbarHeight) *
                        originOverDestinationFrameRatio) -
                       originFrame.size.height;

  UIBezierPath* croppedToolbarsPath =
      CreateTabGridAnimationRoundedRectPathWithInsets(
          animatedView.frame, bottomCrop / originOverDestinationFrameRatio,
          topToolbarHeight, gridCellCornerRadius);

  // Create new path that follows device corner radius, and doesn't crop the
  // toolbars.
  UIBezierPath* noCropPath =
      [UIBezierPath bezierPathWithRoundedRect:animatedView.bounds
                                 cornerRadius:DeviceCornerRadius()];

  // Set the mask on the animated view.
  CAShapeLayer* mask = CreateTabGridAnimationMaskWithFrame(animatedView.frame);
  animatedView.layer.mask = mask;

  // Animate the mask "removal", and from the grid cell corner radius to the
  // device corner radius.
  CASpringAnimation* animation =
      [CASpringAnimation animationWithKeyPath:@"path"];
  animation.initialVelocity = kTabGridTransitionAnimationInitialVelocity;
  animation.fromValue = (id)(croppedToolbarsPath.CGPath);
  animation.toValue = (id)(noCropPath.CGPath);
  animation.duration = kGridToTabAnimationDuration * 0.9;
  animation.timingFunction =
      [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
  animation.removedOnCompletion = false;
  [animatedView.layer.mask addAnimation:animation forKey:@"maskAnimation"];

  // Active tab grid blur animation setup.
  UIVisualEffectView* activeGridBlurView =
      [[UIVisualEffectView alloc] initWithEffect:nil];
  activeGridBlurView.translatesAutoresizingMaskIntoConstraints = NO;
  [animatedView.superview insertSubview:activeGridBlurView
                           belowSubview:animatedView];
  AddSameConstraints(activeGridBlurView.superview, activeGridBlurView);

  [animatedView.superview setNeedsLayout];
  [animatedView.superview layoutIfNeeded];

  // Active grid zoom animation setup. Find the center of the origin frame in
  // the grid view's coordinate system.  This is needed because of the grid
  // view's scroll view.
  SetAnchorPointToFrameCenter(activeGridView, originFrame);
  SetAnchorPointToFrameCenter(pinnedTabsView, originFrame);

  // Perform pre-animations setup.
  animatedView.transform = CGAffineTransformMakeScale(
      originOverDestinationFrameRatio, originOverDestinationFrameRatio);

  if (_animationParameters.shouldScaleTopToolbar) {
    topToolbarSnapshotView.transform = CGAffineTransformMakeScale(0.8, 0.8);
  }
  bottomToolbarSnapshotView.transform = CGAffineTransformMakeScale(0.8, 0.8);

  topToolbarSnapshotView.alpha = 0.5;
  bottomToolbarSnapshotView.alpha = 0.5;

  // Compensate for mask height difference, translate scaled animated view to be
  // top-aligned with the originFrame.
  CGRect translatedNewFrame = animatedView.frame;
  translatedNewFrame.origin = originFrame.origin;
  translatedNewFrame.origin.y -=
      topToolbarHeight * originOverDestinationFrameRatio;
  animatedView.frame = translatedNewFrame;

  // The main tab grid transition animation.
  void (^mainAnimation)() = ^{
    // Animate active grid view's blur.
    activeGridBlurView.effect =
        [UIBlurEffect effectWithStyle:kTabGridBlurStyle];

    // Scale the animated view and content image frames to their destination
    // frames.
    animatedView.transform = CGAffineTransformIdentity;
    animatedView.frame = destinationFrame;
    contentImageView.frame = imageViewDestinationFrame;

    // Scale the active grid view and pinned tabs view (zoom effect).
    activeGridView.transform = CGAffineTransformMakeScale(
        kTabGridAnimationScale, kTabGridAnimationScale);
    pinnedTabsView.transform = CGAffineTransformMakeScale(
        kTabGridAnimationScale, kTabGridAnimationScale);

    // Needed so that the contentImageView's innerImageView frame is animated.
    [contentImageView setNeedsLayout];
    [contentImageView layoutIfNeeded];

    // Animate the content image view's alpha to 0.
    contentImageView.alpha = 0;
  };

  // Toolbars animation
  void (^toolbarsAnimation)() = ^{
    topToolbarSnapshotView.transform = CGAffineTransformIdentity;
    bottomToolbarSnapshotView.transform = CGAffineTransformIdentity;
    topToolbarSnapshotView.alpha = 1.0;
    bottomToolbarSnapshotView.alpha = 1.0;
  };

  if (isActiveCellPinned) {
    animatedView.alpha = 0.1;
  }

  // Fade in the animated view at the beginning of the main animation, only
  // applicable to pinned tabs.
  void (^fadeInPinnedTabCellAnimation)() = ^{
    animatedView.alpha = 1.0;
  };

  // Animate the toolbars with a delay and relative duration.
  void (^relativeStartAndDurationToolbarsAnimation)() = ^{
    [UIView addKeyframeWithRelativeStartTime:
                kGridToTabToolbarsAnimationRelativeStartTime
                            relativeDuration:
                                kGridToTabToolbarsAnimationRelativeDuration
                                  animations:toolbarsAnimation];
  };

  // The completion block for the animation. Also executes the provided
  // completion block.
  void (^animationCompletion)(BOOL) = ^(BOOL finished) {
    // Reset the active grid view.
    activeGridView.transform = CGAffineTransformIdentity;
    CGRect oldAnimationFrame = activeGridView.frame;
    activeGridView.layer.anchorPoint = CGPointMake(0.5, 0.5);
    activeGridView.frame = oldAnimationFrame;

    // Reset the pinned tabs view.
    pinnedTabsView.transform = CGAffineTransformIdentity;
    CGRect oldPinnedTabsFrame = pinnedTabsView.frame;
    pinnedTabsView.layer.anchorPoint = CGPointMake(0.5, 0.5);
    pinnedTabsView.frame = oldPinnedTabsFrame;

    // Reset the animated view.
    animatedView.transform = CGAffineTransformIdentity;
    animatedView.layer.mask = nil;

    // Remove the views added only for the animation.
    [topToolbarBackground removeFromSuperview];
    [bottomToolbarBackground removeFromSuperview];
    [contentImageView removeFromSuperview];
    [activeGridBlurView removeFromSuperview];

    if (completion) {
      completion();
    }
  };

  if (isActiveCellPinned) {
    [UIView animateWithDuration:kGridToTabAnimationDuration / 5
                          delay:0
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:fadeInPinnedTabCellAnimation
                     completion:nil];
  }

  // Perform the toolbars animation.
  [UIView animateKeyframesWithDuration:kGridToTabAnimationDuration
                                 delay:0
                               options:UIViewAnimationOptionCurveEaseInOut
                            animations:relativeStartAndDurationToolbarsAnimation
                            completion:nil];

  // Perform the main animation.
  [UIView animateWithDuration:kGridToTabAnimationDuration
                        delay:0
       usingSpringWithDamping:kGridToTabAnimationDamping
        initialSpringVelocity:kTabGridTransitionAnimationInitialVelocity
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:mainAnimation
                   completion:animationCompletion];
}

@end
