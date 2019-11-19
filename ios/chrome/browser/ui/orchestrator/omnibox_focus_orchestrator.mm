// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/orchestrator/omnibox_focus_orchestrator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/orchestrator/edit_view_animatee.h"
#import "ios/chrome/browser/ui/orchestrator/location_bar_animatee.h"
#import "ios/chrome/browser/ui/orchestrator/toolbar_animatee.h"
#import "ios/chrome/common/material_timing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxFocusOrchestrator ()

@property(nonatomic, assign) BOOL isAnimating;
@property(nonatomic, assign) BOOL stateChangedDuringAnimation;
@property(nonatomic, assign) BOOL finalOmniboxFocusedState;
@property(nonatomic, assign) BOOL finalToolbarExpandedState;
@property(nonatomic, assign) int inProgressAnimationCount;

@end

@implementation OmniboxFocusOrchestrator

- (void)transitionToStateOmniboxFocused:(BOOL)omniboxFocused
                        toolbarExpanded:(BOOL)toolbarExpanded
                               animated:(BOOL)animated {
  // If a new transition is requested while one is ongoing, we don't want
  // to start the new one immediately. However, we do want the omnibox to end
  // up in whatever state was requested last. Therefore, we cache the last
  // requested state and set the omnibox to that state (without animation) at
  // the end of the animations. This may look jerky, but will cause the
  // final state to be a valid one.
  if (self.isAnimating) {
    self.stateChangedDuringAnimation = YES;
    self.finalOmniboxFocusedState = omniboxFocused;
    self.finalToolbarExpandedState = toolbarExpanded;
    return;
  }

  self.isAnimating = animated;
  self.inProgressAnimationCount = 0;

  if (toolbarExpanded) {
    [self updateUIToExpandedState:animated];
  } else {
    [self updateUIToContractedState:animated];
  }

  // Make the rest of the animation happen on the next runloop when this
  // animation have calculated the final frame for the location bar.
  // This is necessary because expanding/contracting the toolbar is actually
  // changing the view layout. Therefore, the expand/contract animations are
  // actually moving views (through modifying the constraints). At the same time
  // the focus/defocus animation don't actually modify the view position, the
  // views remain in place, so it's better to animate them with transforms.
  // The cleanest way to compute and perform the transform animation together
  // with a constraint animation seems to be to let the constraint animation
  // start and compute the final frames, then perform the transform animation.
  dispatch_async(dispatch_get_main_queue(), ^{
    if (omniboxFocused) {
      [self focusOmniboxAnimated:animated];
    } else {
      [self defocusOmniboxAnimated:animated];
    }
  });
}

#pragma mark - Private

- (void)focusOmniboxAnimated:(BOOL)animated {
  // Cleans up after the animation.
  void (^cleanup)() = ^{
    [self.locationBarAnimatee setEditViewHidden:NO];
    [self.locationBarAnimatee setSteadyViewHidden:YES];
    [self.locationBarAnimatee resetTransforms];
    [self.locationBarAnimatee setSteadyViewFaded:NO];
    [self.locationBarAnimatee setEditViewFaded:NO];
    [self.editViewAnimatee setLeadingIconFaded:NO];
    [self.editViewAnimatee setClearButtonFaded:NO];
  };

  if (animated) {
    // Prepare for animation.
    [self.locationBarAnimatee offsetEditViewToMatchSteadyView];
    // Hide badge view before the transform regardless of current displayed
    // state to prevent it from being visible outside of the location bar as the
    // steadView moves outside to the leading side of the location bar.
    [self.locationBarAnimatee hideSteadyViewBadgeView];
    // Make edit view transparent, but not hidden.
    [self.locationBarAnimatee setEditViewHidden:NO];
    [self.locationBarAnimatee setEditViewFaded:YES];
    [self.editViewAnimatee setLeadingIconFaded:YES];
    [self.editViewAnimatee setClearButtonFaded:YES];

    CGFloat duration = ios::material::kDuration1;

    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:duration
        delay:0
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [self.locationBarAnimatee
                  resetEditViewOffsetAndOffsetSteadyViewToMatch];
        }
        completion:^(BOOL complete) {
          cleanup();
          [self animationFinished];
        }];

    // Fading the views happens with a different timing for a better visual
    // effect. The steady view looks like an ordinary label, and it fades before
    // the animation is complete. The edit view will be in pre-edit state, so it
    // looks like selected text. Since the selection is blue, it looks
    // overwhelming if faded in at the same time as the steady view. So it fades
    // in faster and later into the animation to look better.
    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:duration * 0.8
        delay:duration * 0.1
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [self.locationBarAnimatee setSteadyViewFaded:YES];
        }
        completion:^(BOOL complete) {
          [self animationFinished];
        }];

    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:duration * 0.6
        delay:duration * 0.4
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [self.locationBarAnimatee setEditViewFaded:NO];
        }
        completion:^(BOOL _) {
          [self animationFinished];
        }];
    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:duration * 0.2
        delay:duration * 0.8
        options:UIViewAnimationCurveLinear
        animations:^{
          [self.editViewAnimatee setLeadingIconFaded:NO];
          [self.editViewAnimatee setClearButtonFaded:NO];
        }
        completion:^(BOOL _) {
          [self animationFinished];
        }];
  } else {
    cleanup();
  }
}

- (void)defocusOmniboxAnimated:(BOOL)animated {
  // Cleans up after the animation.
  void (^cleanup)() = ^{
    [self.locationBarAnimatee setEditViewHidden:YES];
    [self.locationBarAnimatee setSteadyViewHidden:NO];
    [self.locationBarAnimatee showSteadyViewBadgeView];
    [self.locationBarAnimatee resetTransforms];
    [self.locationBarAnimatee setSteadyViewFaded:NO];
    [self.editViewAnimatee setLeadingIconFaded:NO];
    [self.editViewAnimatee setClearButtonFaded:NO];
  };

  if (animated) {
    // Prepare for animation.
    [self.locationBarAnimatee offsetSteadyViewToMatchEditView];
    // Make steady view transparent, but not hidden.
    [self.locationBarAnimatee setSteadyViewHidden:NO];
    [self.locationBarAnimatee setSteadyViewFaded:YES];
    [self.editViewAnimatee setLeadingIconFaded:NO];
    [self.editViewAnimatee setClearButtonFaded:NO];

    CGFloat duration = ios::material::kDuration1;

    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:duration
        delay:0
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [self.locationBarAnimatee
                  resetSteadyViewOffsetAndOffsetEditViewToMatch];
        }
        completion:^(BOOL _) {
          cleanup();
          [self animationFinished];
        }];

    // These timings are explained in a comment in
    // focusOmniboxAnimated:shouldExpand:.
    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:0.2 * duration
        animations:^{
          [self.editViewAnimatee setLeadingIconFaded:YES];
          [self.editViewAnimatee setClearButtonFaded:YES];
        }
        completion:^(BOOL _) {
          [self animationFinished];
        }];

    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:duration * 0.8
        delay:duration * 0.1
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [self.locationBarAnimatee setEditViewFaded:YES];
        }
        completion:^(BOOL _) {
          [self animationFinished];
        }];

    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:duration * 0.6
        delay:duration * 0.4
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [self.locationBarAnimatee setSteadyViewFaded:NO];
        }
        completion:^(BOOL _) {
          [self animationFinished];
        }];

  } else {
    cleanup();
  }
}

// Updates the UI elements reflect the toolbar expanded state, |animated| or
// not.
- (void)updateUIToExpandedState:(BOOL)animated {
  void (^expansion)() = ^{
    [self.toolbarAnimatee expandLocationBar];
    [self.toolbarAnimatee showCancelButton];
  };

  void (^hideControls)() = ^{
    [self.toolbarAnimatee hideControlButtons];
  };

  if (animated) {
    // Use UIView animateWithDuration instead of UIViewPropertyAnimator to
    // avoid UIKit bug. See https://crbug.com/856155.
    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:ios::material::kDuration1
                          delay:0
                        options:UIViewAnimationCurveEaseInOut
                     animations:expansion
                     completion:^(BOOL _) {
                       [self animationFinished];
                     }];

    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:ios::material::kDuration2
                          delay:0
                        options:UIViewAnimationCurveEaseInOut
                     animations:hideControls
                     completion:^(BOOL _) {
                       [self animationFinished];
                     }];
  } else {
    expansion();
    hideControls();
  }
}

// Updates the UI elements reflect the toolbar contracted state, |animated| or
// not.
- (void)updateUIToContractedState:(BOOL)animated {
  void (^contraction)() = ^{
    [self.toolbarAnimatee contractLocationBar];
  };

  void (^hideCancel)() = ^{
    [self.toolbarAnimatee hideCancelButton];
  };

  void (^showControls)() = ^{
    [self.toolbarAnimatee showControlButtons];
  };

  if (animated) {
    // Use UIView animateWithDuration instead of UIViewPropertyAnimator to
    // avoid UIKit bug. See https://crbug.com/856155.
    CGFloat totalDuration =
        ios::material::kDuration1 + ios::material::kDuration2;
    CGFloat relativeDurationAnimation1 =
        ios::material::kDuration1 / totalDuration;
    self.inProgressAnimationCount += 1;
    [UIView animateKeyframesWithDuration:totalDuration
        delay:0
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [UIView addKeyframeWithRelativeStartTime:0
                                  relativeDuration:relativeDurationAnimation1
                                        animations:^{
                                          contraction();
                                        }];
          [UIView
              addKeyframeWithRelativeStartTime:relativeDurationAnimation1
                              relativeDuration:1 - relativeDurationAnimation1
                                    animations:^{
                                      showControls();
                                    }];
        }
        completion:^(BOOL _) {
          [self animationFinished];
          hideCancel();
        }];
  } else {
    contraction();
    showControls();
    hideCancel();
  }
}

- (void)animationFinished {
  self.inProgressAnimationCount -= 1;
  if (self.inProgressAnimationCount > 0) {
    return;
  }

  // inProgressAnimation count should never be negative because it should
  // always be incremented before starting an animation and decremented
  // when the animation finishes.
  DCHECK(self.inProgressAnimationCount == 0);

  self.isAnimating = NO;
  if (self.stateChangedDuringAnimation) {
    [self transitionToStateOmniboxFocused:self.finalOmniboxFocusedState
                          toolbarExpanded:self.finalToolbarExpandedState
                                 animated:NO];
  }
  self.stateChangedDuringAnimation = NO;
}

@end
