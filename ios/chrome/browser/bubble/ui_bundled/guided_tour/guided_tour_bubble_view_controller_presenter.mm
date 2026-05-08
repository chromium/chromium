// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/guided_tour/guided_tour_bubble_view_controller_presenter.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter+Subclassing.h"
#import "ios/chrome/browser/bubble/ui_bundled/guided_tour/guided_tour_bubble_view_controller_animator.h"
#import "ios/chrome/browser/bubble/ui_bundled/guided_tour/guided_tour_bubble_view_controller_presentation_controller.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"

namespace {

// Margin between the BubbleView and the trailing screen.
const CGFloat kBubbleViewTrailingMarginWithBoundingRect = 25;

BubblePageControlPage BubblePageControlPageForStep(GuidedTourStep step) {
  switch (step) {
    case GuidedTourStep::kNTP:
      return BubblePageControlPageFirst;
    case GuidedTourStep::kTabGridIncognito:
      return BubblePageControlPageSecond;
    case GuidedTourStep::kTabGridLongPress:
      return BubblePageControlPageThird;
    case GuidedTourStep::kTabGridTabGroup:
      return BubblePageControlPageFourth;
  }
}

}  // namespace

@interface GuidedTourBubbleViewControllerPresenter () <
    GuidedTourBubbleViewPositioner,
    UIViewControllerTransitioningDelegate>
@end

@implementation GuidedTourBubbleViewControllerPresenter {
  UIViewController* _parentViewController;
  CGPoint _anchorPointInParent;
  ProceduralBlock _completionCallback;
  CGFloat _cornerRadius;
  BubblePageControlPage _page;
  UIView* _anchorView;
}

- (instancetype)initWithText:(NSString*)text
                           title:(NSString*)titleString
                  guidedTourStep:(GuidedTourStep)step
                  arrowDirection:(BubbleArrowDirection)arrowDirection
                       alignment:(BubbleAlignment)alignment
                      bubbleType:(BubbleViewType)type
    backgroundCutoutCornerRadius:(CGFloat)cornerRadius
              completionCallback:(ProceduralBlock)completionCallback {
  self = [super initWithText:text
                       title:titleString
              arrowDirection:arrowDirection
                   alignment:alignment
                  bubbleType:type
             pageControlPage:BubblePageControlPageForStep(step)
       customNextButtonTitle:nil
           dismissalCallback:nil];
  if (self) {
    _completionCallback = completionCallback;
    _cornerRadius = cornerRadius;
  }
  return self;
}

- (void)presentInViewController:(UIViewController*)parentViewController
                    anchorPoint:(CGPoint)anchorPoint
                     anchorView:(UIView*)anchorView {
  _anchorView = anchorView;
  [self.bubbleViewController displayAnimated:NO];
  [self
      configureInParentViewController:parentViewController
                          anchorPoint:anchorPoint
                      anchorViewFrame:[anchorView convertRect:anchorView.bounds
                                                       toView:nil]];
  _parentViewController = parentViewController;
  _anchorPointInParent = [self.parentView.window convertPoint:anchorPoint
                                                       toView:self.parentView];

  self.bubbleViewController.modalPresentationStyle = UIModalPresentationCustom;
  self.bubbleViewController.transitioningDelegate = self;
  [parentViewController presentViewController:self.bubbleViewController
                                     animated:YES
                                   completion:nil];

  [self registerVoiceOverAnnouncement];
}

- (void)dismiss {
  if (!self.presenting) {
    return;
  }
  [_parentViewController dismissViewControllerAnimated:YES completion:nil];
  self.presenting = NO;
  if (_completionCallback) {
    _completionCallback();
  }
}

#pragma mark - BubbleViewControllerPresenter

- (CGRect)frameForBubbleInRect:(CGRect)rect atAnchorPoint:(CGPoint)anchorPoint {
  // Deduct from the trailing end to ensure the BubbleView has space in between
  // the screen boundary.
  rect.size.width -= kBubbleViewTrailingMarginWithBoundingRect;
  return [super frameForBubbleInRect:rect atAnchorPoint:anchorPoint];
}

#pragma mark - BubbleViewDelegate

- (void)didTapNextButton {
  [self dismiss];
}

#pragma mark - UIViewControllerTransitioningDelegate

- (nullable UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (nullable UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  GuidedTourBubbleViewControllerPresentationController* controller =
      [[GuidedTourBubbleViewControllerPresentationController alloc]
          initWithPresentedViewController:self.bubbleViewController
                 presentingViewController:_parentViewController
                               anchorView:_anchorView
                             cornerRadius:_cornerRadius];
  controller.positioner = self;
  return controller;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  GuidedTourBubbleViewControllerAnimator* animator =
      [[GuidedTourBubbleViewControllerAnimator alloc] init];
  animator.appearing = YES;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  GuidedTourBubbleViewControllerAnimator* animator =
      [[GuidedTourBubbleViewControllerAnimator alloc] init];
  animator.appearing = NO;
  return animator;
}

#pragma mark - GuidedTourBubbleViewPositioner

- (CGRect)presentedBubbleViewFrame {
  CGPoint anchorPoint = [self.delegate
      anchorPointForGuidedTourBubbleViewControllerPresenter:self];
  _anchorPointInParent = [self.parentView.window convertPoint:anchorPoint
                                                       toView:self.parentView];
  return [self frameForBubbleInRect:self.parentView.bounds
                      atAnchorPoint:_anchorPointInParent];
}

@end
