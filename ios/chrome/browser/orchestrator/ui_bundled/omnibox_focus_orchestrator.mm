// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/orchestrator/ui_bundled/omnibox_focus_orchestrator.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/edit_view_animatee.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/location_bar_animatee.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/toolbar_animatee.h"
#import "ios/chrome/common/material_timing.h"

@interface OmniboxFocusOrchestrator ()

@property(nonatomic, assign) BOOL isAnimating;
@property(nonatomic, assign) BOOL stateChangedDuringAnimation;
@property(nonatomic, assign) BOOL finalOmniboxFocusedState;
@property(nonatomic, assign) BOOL finalToolbarExpandedState;
@property(nonatomic, assign) int inProgressAnimationCount;

// Sometimes, the toolbar animations finish before the omnibox animations are
// even queued, causing the final completions to be run too early.
@property(nonatomic, assign) BOOL areOmniboxChangesQueued;

@end

@implementation OmniboxFocusOrchestrator {
  ProceduralBlock _completion;
  OmniboxFocusTrigger _trigger;
}

- (void)transitionToStateOmniboxFocused:(BOOL)omniboxFocused
                        toolbarExpanded:(BOOL)toolbarExpanded
                                trigger:(OmniboxFocusTrigger)trigger
                               animated:(BOOL)animated
                             completion:(ProceduralBlock)completion {
  _completion = completion;
  _trigger = trigger;
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
  self.areOmniboxChangesQueued = NO;
  self.inProgressAnimationCount = 0;

  if (omniboxFocused) {
    [self prepareToFocusOmniboxAnimated:animated];
  }

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
    self.areOmniboxChangesQueued = YES;
    if (omniboxFocused) {
      [self focusOmniboxAnimated:animated];
    } else {
      [self defocusOmniboxAnimated:animated];
    }

    // Make sure that some omnibox animations were queued. Otherwise, the final
    // call to `animationFinished` after the toolbar animations finished was
    // interrupted and cleanup still needs to occur.
    if (self.inProgressAnimationCount == 0 && self.isAnimating) {
      [self cleanupAfterAnimations];
    }
  });
}

#pragma mark - Private

// Sets some initial state that needs to be set immediately, before any
// `dispatch_async` calls, in order to avoid flicker at the start of the
// animation.
- (void)prepareToFocusOmniboxAnimated:(BOOL)animated {
  if (!animated) {
    return;
  }

  if ([self isTriggerUnpinnedFakebox]) {
    // If focus trigger is the unpinned fakebox, the edit view will appear
    // in-place (without animation) and the steady view will not slide and
    // fade out - it will be hidden from the start.
    [UIView performWithoutAnimation:^{
      // This can be triggered inside of another animation on the NTP, so
      // `performWithoutAnimation` is used to ensure that these changes happen
      // immediately.
      [self.locationBarAnimatee resetTextFieldOffsetAndOffsetSteadyViewToMatch];
      [self.locationBarAnimatee setEditViewFaded:NO];
      [self.locationBarAnimatee setSteadyViewFaded:YES];
    }];
  }
}

- (void)focusOmniboxAnimated:(BOOL)animated {
  // Cleans up after the animation.
  void (^cleanup)() = ^{
    [self.locationBarAnimatee setEditViewHidden:NO];
    [self.locationBarAnimatee setSteadyViewHidden:YES];
    [self.locationBarAnimatee resetTransforms];
    [self.locationBarAnimatee setSteadyViewFaded:NO];
    [self.locationBarAnimatee setEditViewFaded:NO];
    [self.editViewAnimatee setLeadingIconScale:1];
    [self.editViewAnimatee setClearButtonFaded:NO];
  };

  if (animated) {
    // Prepare for animation.
    BOOL shouldCrossfadeEditAndSteadyViews = ![self isTriggerUnpinnedFakebox];
    if (shouldCrossfadeEditAndSteadyViews) {
      [self.locationBarAnimatee offsetTextFieldToMatchSteadyView];
      [self.locationBarAnimatee setEditViewFaded:YES];
    }

    // Hide badge and entrypoint views before the transform regardless of
    // current displayed state to prevent them from being visible outside of the
    // location bar as the steadView moves outside to the leading side of the
    // location bar.
    [self.locationBarAnimatee hideSteadyViewBadgeAndEntrypointViews];
    // Make edit view transparent, but not hidden.
    [self.locationBarAnimatee setEditViewHidden:NO];
    [self.editViewAnimatee setLeadingIconScale:0];
    [self.editViewAnimatee setClearButtonFaded:YES];

    self.inProgressAnimationCount += 1;
    [UIView animateKeyframesWithDuration:kMaterialDuration1
        delay:0
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          if (shouldCrossfadeEditAndSteadyViews) {
            [self.locationBarAnimatee
                    resetTextFieldOffsetAndOffsetSteadyViewToMatch];

            // Fading the views happens with a different timing for a better
            // visual effect. The steady view looks like an ordinary label, and
            // it fades before the animation is complete. The edit view will be
            // in pre-edit state, so it looks like selected text. Since the
            // selection is blue, it looks overwhelming if faded in at the same
            // time as the steady view. So it fades in faster and later into the
            // animation to look better.
            [UIView addKeyframeWithRelativeStartTime:0.1
                                    relativeDuration:0.8
                                          animations:^{
                                            [self.locationBarAnimatee
                                                setSteadyViewFaded:YES];
                                          }];

            [UIView addKeyframeWithRelativeStartTime:0.4
                                    relativeDuration:0.6
                                          animations:^{
                                            [self.locationBarAnimatee
                                                setEditViewFaded:NO];
                                          }];
          }

          // Scale the leading icon in with a slight bounce / spring.
          [UIView addKeyframeWithRelativeStartTime:0
                                  relativeDuration:0.75
                                        animations:^{
                                          [self.editViewAnimatee
                                              setLeadingIconScale:1.3];
                                        }];
          [UIView addKeyframeWithRelativeStartTime:0.75
                                  relativeDuration:0.25
                                        animations:^{
                                          [self.editViewAnimatee
                                              setLeadingIconScale:1];
                                          [self.editViewAnimatee
                                              setClearButtonFaded:NO];
                                        }];
        }
        completion:^(BOOL finished) {
          cleanup();
          [self animationFinished];
        }];
  } else {
    cleanup();

    if (_completion) {
      _completion();
      _completion = nil;
    }
  }
}

- (void)defocusOmniboxAnimated:(BOOL)animated {
  // Cleans up after the animation.
  void (^cleanup)() = ^{
    [self.locationBarAnimatee setEditViewHidden:YES];
    [self.locationBarAnimatee setSteadyViewHidden:NO];
    [self.locationBarAnimatee showSteadyViewBadgeAndEntrypointViews];
    [self.locationBarAnimatee resetTransforms];
    [self.locationBarAnimatee setSteadyViewFaded:NO];
    [self.editViewAnimatee setLeadingIconScale:1];
    [self.editViewAnimatee setClearButtonFaded:NO];
  };

  if (animated) {
    // Prepare for animation.
    [self.locationBarAnimatee offsetSteadyViewToMatchTextField];
    // Make steady view transparent, but not hidden.
    [self.locationBarAnimatee setSteadyViewHidden:NO];
    [self.locationBarAnimatee setSteadyViewFaded:YES];
    [self.editViewAnimatee setLeadingIconScale:1];
    [self.editViewAnimatee setClearButtonFaded:NO];
    CGFloat duration = kMaterialDuration1;

    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:duration
        delay:0
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [self.locationBarAnimatee
                  resetSteadyViewOffsetAndOffsetTextFieldToMatch];
        }
        completion:^(BOOL finished) {
          cleanup();
          [self animationFinished];
        }];

    // These timings are explained in a comment in
    // focusOmniboxAnimated:shouldExpand:.
    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:0.2 * duration
        animations:^{
          [self.editViewAnimatee setLeadingIconScale:0];
          [self.editViewAnimatee setClearButtonFaded:YES];
        }
        completion:^(BOOL finished) {
          [self animationFinished];
        }];

    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:duration * 0.8
        delay:duration * 0.1
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [self.locationBarAnimatee setEditViewFaded:YES];
        }
        completion:^(BOOL finished) {
          [self animationFinished];
        }];

    self.inProgressAnimationCount += 1;
    [UIView animateWithDuration:duration * 0.6
        delay:duration * 0.4
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [self.locationBarAnimatee setSteadyViewFaded:NO];
        }
        completion:^(BOOL finished) {
          [self animationFinished];
        }];

  } else {
    cleanup();

    if (_completion) {
      _completion();
      _completion = nil;
    }
  }
}

// Updates the UI elements reflect the toolbar expanded state, `animated` or
// not.
- (void)updateUIToExpandedState:(BOOL)animated {
  if (animated) {
    // Use UIView animateWithDuration instead of UIViewPropertyAnimator to
    // avoid UIKit bug. See https://crbug.com/856155.
    self.inProgressAnimationCount += 1;
    if (IsIOSLargeFakeboxEnabled()) {
      // Set the location bar height to the default.
      [self.toolbarAnimatee setLocationBarHeightExpanded];
    }
    [self.toolbarAnimatee setToolbarFaded:NO];
    switch (_trigger) {
      case OmniboxFocusTrigger::kPinnedLargeFakebox:
        [self.toolbarAnimatee setLocationBarHeightToMatchFakeOmnibox];
        break;
      case OmniboxFocusTrigger::kUnpinnedLargeFakebox:
      case OmniboxFocusTrigger::kUnpinnedFakebox:
        [self.toolbarAnimatee setToolbarFaded:YES];
        break;
      default:
        break;
    }
    [UIView animateKeyframesWithDuration:kMaterialDuration1
        delay:0
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [UIView addKeyframeWithRelativeStartTime:0
                                  relativeDuration:1
                                        animations:^{
                                          [self expansion];
                                        }];
          [UIView
              addKeyframeWithRelativeStartTime:0
                              relativeDuration:kMaterialDuration2 /
                                               kMaterialDuration1
                                    animations:^{
                                      [self.toolbarAnimatee hideControlButtons];
                                    }];
        }
        completion:^(BOOL finished) {
          [self animationFinished];
        }];

  } else {
    [self expansion];
    [self.toolbarAnimatee hideControlButtons];
  }
}

// Updates the UI elements reflect the toolbar contracted state, `animated` or
// not.
- (void)updateUIToContractedState:(BOOL)animated {
  if (animated) {
    // Use UIView animateWithDuration instead of UIViewPropertyAnimator to
    // avoid UIKit bug. See https://crbug.com/856155.
    CGFloat totalDuration = kMaterialDuration1 + kMaterialDuration2;
    CGFloat relativeDurationAnimation1 = kMaterialDuration1 / totalDuration;
    self.inProgressAnimationCount += 1;
    [UIView animateKeyframesWithDuration:totalDuration
        delay:0
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [UIView addKeyframeWithRelativeStartTime:0
                                  relativeDuration:relativeDurationAnimation1
                                        animations:^{
                                          [self contraction];
                                        }];
          [UIView
              addKeyframeWithRelativeStartTime:relativeDurationAnimation1
                              relativeDuration:1 - relativeDurationAnimation1
                                    animations:^{
                                      [self.toolbarAnimatee showControlButtons];
                                    }];
        }
        completion:^(BOOL finished) {
          [self.toolbarAnimatee hideCancelButton];
          [self animationFinished];
        }];
  } else {
    [self contraction];
    [self.toolbarAnimatee showControlButtons];
    [self.toolbarAnimatee hideCancelButton];
  }
}

- (void)animationFinished {
  self.inProgressAnimationCount -= 1;
  [self cleanupAfterAnimations];
}

- (void)cleanupAfterAnimations {
  // Make sure all the animations have been queued and finished.
  if (!self.areOmniboxChangesQueued || self.inProgressAnimationCount > 0) {
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
                                  trigger:_trigger
                                 animated:NO
                               completion:_completion];
  } else {
    if (_completion) {
      _completion();
      _completion = nil;
    }
    if (IsIOSLargeFakeboxEnabled()) {
      // Reset the location bar height back to the default.
      [self.toolbarAnimatee setLocationBarHeightExpanded];
    }
  }
  self.stateChangedDuringAnimation = NO;
}

#pragma mark - Private animation helpers

// Visually expands the location bar for focus.
- (void)expansion {
  [self.toolbarAnimatee expandLocationBar];
  [self.toolbarAnimatee showCancelButton];
  switch (_trigger) {
    case OmniboxFocusTrigger::kPinnedLargeFakebox:
      [self.toolbarAnimatee setLocationBarHeightExpanded];
      break;
    case OmniboxFocusTrigger::kUnpinnedLargeFakebox:
    case OmniboxFocusTrigger::kUnpinnedFakebox:
      [self.toolbarAnimatee setToolbarFaded:NO];
      break;
    default:
      break;
  }
}

// Visually contracts the location bar for defocus.
- (void)contraction {
  [self.toolbarAnimatee contractLocationBar];
  if (_trigger == OmniboxFocusTrigger::kPinnedLargeFakebox) {
    [self.toolbarAnimatee setLocationBarHeightToMatchFakeOmnibox];
  }
}

// Returns YES if the focus event was triggered by the NTP Fakebox in its
// unpinned state.
- (BOOL)isTriggerUnpinnedFakebox {
  switch (_trigger) {
    case OmniboxFocusTrigger::kUnpinnedLargeFakebox:
    case OmniboxFocusTrigger::kUnpinnedFakebox:
      return YES;
    case OmniboxFocusTrigger::kOther:
    case OmniboxFocusTrigger::kPinnedFakebox:
    case OmniboxFocusTrigger::kPinnedLargeFakebox:
      return NO;
  }
}
@end
