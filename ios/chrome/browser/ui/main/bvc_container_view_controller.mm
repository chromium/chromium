// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"

#import <ostream>

#import "base/check_op.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_constants.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BVCContainerViewController ()

// The thumb strip's pan gesture handler that will be added to the toolbar and
// tab strip.
@property(nonatomic, weak)
    ViewRevealingVerticalPanHandler* thumbStripPanHandler;

// Background behind toolbar and webview during animation to avoid seeing
// partial color streaks from background with different color between the
// different moving parts.
@property(nonatomic, strong) UIView* solidBackground;

@end

@implementation BVCContainerViewController

#pragma mark - Public

- (UIViewController*)currentBVC {
  return [self.childViewControllers firstObject];
}

- (void)setCurrentBVC:(UIViewController*)bvc {
  // When the thumb strip is enabled, the BVC container stays around all the
  // time. When on a tab grid page with no tabs or the recent tab page, the
  // currentBVC will be set to nil.
  DCHECK(bvc || self.isThumbStripEnabled);
  if (self.currentBVC == bvc) {
    return;
  }

  // Remove the current bvc, if any.
  if (self.currentBVC) {
    [self.currentBVC willMoveToParentViewController:nil];
    [self.currentBVC.view removeFromSuperview];
    [self.currentBVC removeFromParentViewController];
  }

  DCHECK_EQ(nil, self.currentBVC);
  DCHECK_EQ(self.isThumbStripEnabled ? 1U : 0U, self.view.subviews.count);

  // Add the new active view controller.
  if (bvc) {
    [self addChildViewController:bvc];
    // If the BVC's view has a transform, then its frame isn't accurate.
    // Instead, remove the transform, set the frame, then reapply the transform.
    CGAffineTransform oldTransform = bvc.view.transform;
    bvc.view.transform = CGAffineTransformIdentity;
    bvc.view.frame = self.view.bounds;
    bvc.view.transform = oldTransform;
    [self.view addSubview:bvc.view];
    [bvc didMoveToParentViewController:self];
  }

  DCHECK(self.currentBVC == bvc);
}

#pragma mark - UIViewController methods

- (void)viewDidLoad {
  [super viewDidLoad];
}

- (void)presentViewController:(UIViewController*)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^)())completion {
  // Force presentation to go through the current BVC, if possible, which does
  // some associated bookkeeping.
  UIViewController* viewController =
      self.currentBVC ? self.currentBVC : self.fallbackPresenterViewController;
  [viewController presentViewController:viewControllerToPresent
                               animated:flag
                             completion:completion];
}

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)())completion {
  // Force dismissal to go through the current BVC, if possible, which does some
  // associated bookkeeping.
  UIViewController* viewController =
      self.currentBVC ? self.currentBVC : self.fallbackPresenterViewController;
  [viewController dismissViewControllerAnimated:flag completion:completion];
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.currentBVC;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.currentBVC;
}

- (BOOL)shouldAutorotate {
  return self.currentBVC ? [self.currentBVC shouldAutorotate]
                         : [super shouldAutorotate];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  if (self.thumbStripEnabled) {
    DCHECK(self.thumbStripPanHandler);
    CGFloat baseViewHeight = size.height;
    self.thumbStripPanHandler.baseViewHeight = baseViewHeight;
    // On rotation, reposition the BVC container if the state is currently
    // Revealed.
    if (self.thumbStripPanHandler.currentState == ViewRevealState::Revealed) {
      self.view.transform = CGAffineTransformMakeTranslation(
          0, self.thumbStripPanHandler.baseViewHeight);
    }

    [coordinator
        animateAlongsideTransition:nil
                        completion:^(
                            id<UIViewControllerTransitionCoordinatorContext>
                                context) {
                          if (self.thumbStripPanHandler.currentState ==
                              ViewRevealState::Peeked) {
                            CGRect frame = self.view.frame;
                            CGFloat topOffset =
                                self.view.window.safeAreaInsets.top;
                            frame.size.height =
                                topOffset + kTabStripHeight +
                                self.thumbStripPanHandler.baseViewHeight -
                                self.thumbStripPanHandler.peekedHeight;
                            self.view.frame = frame;
                          }
                        }];
  }
}

#pragma mark - ThumbStripSupporting

- (BOOL)isThumbStripEnabled {
  return self.thumbStripPanHandler != nil;
}

- (void)thumbStripEnabledWithPanHandler:
    (ViewRevealingVerticalPanHandler*)panHandler {
  DCHECK(!self.thumbStripEnabled);
  self.solidBackground = [[UIView alloc] initWithFrame:self.view.bounds];
  self.solidBackground.translatesAutoresizingMaskIntoConstraints = NO;
  self.solidBackground.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  self.solidBackground.hidden = YES;
  [self.view addSubview:self.solidBackground];
  self.thumbStripPanHandler = panHandler;
  [panHandler addAnimatee:self];
}

- (void)thumbStripDisabled {
  DCHECK(self.thumbStripEnabled);
  [self.solidBackground removeFromSuperview];
  self.solidBackground = nil;
  self.view.transform = CGAffineTransformIdentity;
  self.thumbStripPanHandler = nil;
}

#pragma mark - ViewRevealingAnimatee

- (void)willAnimateViewRevealFromState:(ViewRevealState)currentViewRevealState
                               toState:(ViewRevealState)nextViewRevealState {
  [self.view sendSubviewToBack:self.solidBackground];
  self.solidBackground.hidden = NO;
  self.solidBackground.overrideUserInterfaceStyle =
      self.incognito ? UIUserInterfaceStyleDark
                     : UIUserInterfaceStyleUnspecified;

  if (currentViewRevealState == ViewRevealState::Peeked) {
    CGRect frame = self.view.frame;
    frame.size.height = self.thumbStripPanHandler.baseViewHeight;
    self.view.frame = frame;
  }
}

- (void)animateViewReveal:(ViewRevealState)nextViewRevealState {
  CGFloat topOffset = self.view.window.safeAreaInsets.top;
  DCHECK(self.thumbStripPanHandler);
  switch (nextViewRevealState) {
    case ViewRevealState::Hidden:
      self.view.transform = CGAffineTransformIdentity;
      self.solidBackground.transform =
          CGAffineTransformMakeTranslation(0, topOffset + kTabStripHeight);
      break;
    case ViewRevealState::Peeked:
      self.view.transform = CGAffineTransformMakeTranslation(
          0, self.thumbStripPanHandler.peekedHeight);
      self.solidBackground.transform =
          CGAffineTransformMakeTranslation(0, topOffset);
      break;
    case ViewRevealState::Revealed:
      self.view.transform = CGAffineTransformMakeTranslation(
          0, self.thumbStripPanHandler.baseViewHeight);
      self.solidBackground.transform =
          CGAffineTransformMakeTranslation(0, topOffset);
      break;
  }
}

- (void)didAnimateViewRevealFromState:(ViewRevealState)startViewRevealState
                              toState:(ViewRevealState)currentViewRevealState
                              trigger:(ViewRevealTrigger)trigger {
  self.solidBackground.hidden = YES;

  if (currentViewRevealState == ViewRevealState::Peeked) {
    // For a11y scroll to work in peeked mode, the frame has to be reduced to
    // the height visible. Otherwise focus goes below the bottom.
    CGFloat topOffset = self.view.window.safeAreaInsets.top;
    CGRect frame = self.view.frame;
    frame.size.height = topOffset + kTabStripHeight +
                        self.thumbStripPanHandler.baseViewHeight -
                        self.thumbStripPanHandler.peekedHeight;
    self.view.frame = frame;
  }
}

@end
