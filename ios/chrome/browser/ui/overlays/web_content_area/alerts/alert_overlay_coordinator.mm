// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/alerts/alert_overlay_coordinator.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/ui/alert_view/alert_view_controller.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/alerts/alert_overlay_mediator.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/ui/presenters/non_modal_view_controller_presenter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertRequest;

@interface AlertOverlayCoordinator () <AlertOverlayMediatorDataSource,
                                       ContainedPresenterDelegate>
@property(nonatomic) AlertViewController* alertViewController;
@property(nonatomic) AlertOverlayMediator* alertMediator;
@property(nonatomic) NonModalViewControllerPresenter* presenter;
@end

@implementation AlertOverlayCoordinator

#pragma mark - Accessors

- (void)setAlertMediator:(AlertOverlayMediator*)alertMediator {
  if ([self.alertMediator isEqual:alertMediator])
    return;
  self.alertMediator.dataSource = nil;
  self.mediator = alertMediator;
  self.alertMediator.dataSource = self;
}

- (AlertOverlayMediator*)alertMediator {
  return base::mac::ObjCCastStrict<AlertOverlayMediator>(self.mediator);
}

#pragma mark - AlertOverlayMediatorDataSource

- (NSString*)textFieldInputForMediator:(AlertOverlayMediator*)mediator
                        textFieldIndex:(NSUInteger)index {
  NSArray<NSString*>* textFieldResults =
      self.alertViewController.textFieldResults;
  return index < textFieldResults.count ? textFieldResults[index] : nil;
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

+ (BOOL)showsOverlayUsingChildViewController {
  return YES;
}

+ (const OverlayRequestSupport*)requestSupport {
  return AlertRequest::RequestSupport();
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
  self.alertMediator =
      [[AlertOverlayMediator alloc] initWithRequest:self.request];
  self.alertMediator.consumer = self.alertViewController;
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

  self.started = NO;
  [self.presenter dismissAnimated:animated];
}

@end
