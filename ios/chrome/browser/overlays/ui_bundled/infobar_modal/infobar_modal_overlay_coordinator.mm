// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_util.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"

@interface InfobarModalOverlayCoordinator () <InfobarModalPositioner>
// The navigation controller used to display the modal view.
@property(nonatomic) UINavigationController* modalNavController;
// The transition delegate used by the coordinator to present the modal UI.
@property(nonatomic, strong)
    InfobarModalTransitionDriver* modalTransitionDriver;
@end

@implementation InfobarModalOverlayCoordinator

#pragma mark - OverlayRequestCoordinator

- (void)startAnimated:(BOOL)animated {
  if (self.started || !self.request)
    return;
  [self configureModal];
  [self configureViewController];
  __weak InfobarModalOverlayCoordinator* weakSelf = self;
  [self.baseViewController presentViewController:self.viewController
                                        animated:animated
                                      completion:^{
                                        [weakSelf finishPresentation];
                                      }];
  self.started = YES;
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started)
    return;
  // Mark started as NO before calling dismissal callback to prevent dup
  // stopAnimated: executions.
  self.started = NO;
  __weak InfobarModalOverlayCoordinator* weakSelf = self;
  [self.baseViewController dismissViewControllerAnimated:animated
                                              completion:^{
                                                [weakSelf finishDismissal];
                                              }];
}

- (UIViewController*)viewController {
  return self.modalNavController;
}

#pragma mark - InfobarModalPositioner

- (CGFloat)modalHeightForWidth:(CGFloat)width {
  UIView* modalView = self.modalViewController.view;
  CGSize modalContentSize = CGSizeZero;
  if (UIScrollView* scrollView =
          base::apple::ObjCCast<UIScrollView>(modalView)) {
    CGRect layoutFrame = self.baseViewController.view.bounds;
    layoutFrame.size.width = width;
    scrollView.frame = layoutFrame;
    [scrollView setNeedsLayout];
    [scrollView layoutIfNeeded];
    modalContentSize = scrollView.contentSize;
  } else {
    modalContentSize = [modalView sizeThatFits:CGSizeMake(width, CGFLOAT_MAX)];
  }
  return modalContentSize.height +
         CGRectGetHeight(self.modalNavController.navigationBar.bounds);
}

#pragma mark - InfobarModalPresentationHandler

- (void)resizeInfobarModal {
  UIView* containerView = self.modalNavController.view;
  containerView.frame = ContainedModalFrameThatFit(self, containerView);
}

#pragma mark - Private

// Called when the presentation of the modal UI is completed.
- (void)finishPresentation {
  // Notify the presentation context that the presentation has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  if (self.delegate) {
    self.delegate->OverlayUIDidFinishPresentation(self.request);
  }
}

// Called when the dismissal of the modal UI is finished.
- (void)finishDismissal {
  [self resetModal];
  self.modalNavController = nil;
  // Notify the presentation context that the dismissal has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  if (self.delegate) {
    self.delegate->OverlayUIDidFinishDismissal(self.request);
  }
}

@end

@implementation InfobarModalOverlayCoordinator (ModalConfiguration)

- (OverlayRequestMediator*)modalMediator {
  NOTREACHED_IN_MIGRATION() << "Subclasses implement.";
  return nullptr;
}

- (UIViewController*)modalViewController {
  NOTREACHED_IN_MIGRATION() << "Subclasses implement.";
  return nil;
}

- (void)configureModal {
  NOTREACHED_IN_MIGRATION() << "Subclasses implement.";
}

- (void)configureViewController {
  self.mediator = self.modalMediator;
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBase];
  self.modalTransitionDriver.modalPositioner = self;
  self.modalNavController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  self.modalNavController.modalPresentationStyle = UIModalPresentationCustom;
  self.modalNavController.transitioningDelegate = self.modalTransitionDriver;
  UINavigationBarAppearance* opaqueAppearance =
      [[UINavigationBarAppearance alloc] init];
  [opaqueAppearance configureWithOpaqueBackground];
  self.modalNavController.navigationBar.standardAppearance = opaqueAppearance;
  self.modalNavController.navigationBar.compactAppearance = opaqueAppearance;
  self.modalNavController.navigationBar.scrollEdgeAppearance = opaqueAppearance;
}

- (void)resetModal {
  NOTREACHED_IN_MIGRATION() << "Subclasses implement.";
}

@end
