// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/ui/presenters/non_modal_view_controller_presenter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AlertOverlayCoordinator () <AlertOverlayMediatorDataSource,
                                       AlertOverlayMediatorDelegate,
                                       ContainedPresenterDelegate>
@property(nonatomic, getter=isStarted) BOOL started;
@property(nonatomic) AlertViewController* alertViewController;
@property(nonatomic) NonModalViewControllerPresenter* presenter;
@property(nonatomic) AlertOverlayMediator* mediator;
@end

@implementation AlertOverlayCoordinator

#pragma mark - Accessors

- (void)setMediator:(AlertOverlayMediator*)mediator {
  if (_mediator == mediator)
    return;
  _mediator.delegate = nil;
  _mediator.dataSource = nil;
  _mediator = mediator;
  _mediator.delegate = self;
  _mediator.dataSource = self;
}

#pragma mark - AlertOverlayMediatorDataSource

- (NSString*)textFieldInputForMediator:(AlertOverlayMediator*)mediator
                        textFieldIndex:(NSUInteger)index {
  NSArray<NSString*>* textFieldResults =
      self.alertViewController.textFieldResults;
  return index < textFieldResults.count ? textFieldResults[index] : nil;
}

#pragma mark - AlertOverlayMediatorDelegate

- (void)stopDialogForMediator:(AlertOverlayMediator*)mediator {
  [self stopAnimated:YES];
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  self.delegate->OverlayUIDidFinishPresentation(self.request);
}

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  self.alertViewController = nil;
  self.presenter = nil;
  self.delegate->OverlayUIDidFinishDismissal(self.request);
}

#pragma mark - OverlayRequestCoordinator

+ (BOOL)supportsRequest:(OverlayRequest*)request {
  NOTREACHED() << "Subclasses implement.";
  return NO;
}

+ (BOOL)usesChildViewController {
  return YES;
}

- (UIViewController*)viewController {
  return self.alertViewController;
}

- (void)startAnimated:(BOOL)animated {
  if (self.started)
    return;
  self.alertViewController = [[AlertViewController alloc] init];
  self.alertViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  self.alertViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  self.mediator = [self newMediator];
  self.mediator.consumer = self.alertViewController;
  self.presenter = [[NonModalViewControllerPresenter alloc] init];
  self.presenter.delegate = self;
  self.presenter.baseViewController = self.baseViewController;
  self.presenter.presentedViewController = self.alertViewController;
  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:animated];
  self.started = YES;
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started)
    return;
  [self.presenter dismissAnimated:animated];
  self.started = NO;
}

@end

@implementation AlertOverlayCoordinator (Subclassing)

- (AlertOverlayMediator*)newMediator {
  NOTREACHED() << "Subclasses implement.";
  return nil;
}

@end
