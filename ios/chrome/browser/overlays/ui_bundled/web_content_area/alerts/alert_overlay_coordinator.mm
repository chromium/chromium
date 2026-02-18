// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/alerts/alert_overlay_coordinator.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/alerts/alert_overlay_mediator.h"
#import "ios/chrome/browser/presenters/ui_bundled/contained_presenter_delegate.h"
#import "ios/chrome/browser/presenters/ui_bundled/non_modal_view_controller_presenter.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

using alert_overlays::AlertRequest;

@interface AlertOverlayCoordinator () <AlertOverlayMediatorDataSource,
                                       ContainedPresenterDelegate>
@property(nonatomic) AlertViewController* alertViewController;
@property(nonatomic) AlertOverlayMediator* alertMediator;
@property(nonatomic) NonModalViewControllerPresenter* presenter;
@end

@implementation AlertOverlayCoordinator {
  // Handler for Gemini commands.
  __weak id<BWGCommands> _geminiHandler;
}

#pragma mark - Accessors

- (void)setAlertMediator:(AlertOverlayMediator*)alertMediator {
  if ([self.alertMediator isEqual:alertMediator]) {
    return;
  }
  self.alertMediator.dataSource = nil;
  self.mediator = alertMediator;
  self.alertMediator.dataSource = self;
}

- (AlertOverlayMediator*)alertMediator {
  return base::apple::ObjCCastStrict<AlertOverlayMediator>(self.mediator);
}

#pragma mark - AlertOverlayMediatorDataSource

- (NSString*)textFieldInputForMediator:(AlertOverlayMediator*)mediator
                        textFieldIndex:(NSUInteger)index {
  NSArray<NSString*>* textFieldResults =
      self.alertViewController.textFieldResults;
  return index < textFieldResults.count ? textFieldResults[index] : nil;
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterWillPresent:(id<ContainedPresenter>)presenter {
  if (IsGeminiCopresenceEnabled()) {
    [_geminiHandler
        hideFloatyIfInvokedAnimated:NO
                         fromSource:gemini::FloatyUpdateSource::Alert];
  }
}

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  self.delegate->OverlayUIDidFinishPresentation(self.request);
}

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  self.alertViewController = nil;
  self.presenter = nil;
  if (IsGeminiCopresenceEnabled()) {
    [_geminiHandler
        updateFloatyVisibilityIfEligibleAnimated:NO
                                      fromSource:gemini::FloatyUpdateSource::
                                                     Alert];
  }
  _geminiHandler = nil;
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
  if (self.started) {
    return;
  }

  if (IsGeminiCopresenceEnabled()) {
    _geminiHandler =
        HandlerForProtocol(self.browser->GetCommandDispatcher(), BWGCommands);
  }
  self.alertViewController = [[AlertViewController alloc] init];
  self.alertViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  self.alertViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  self.alertViewController.actionButtonsAreInitiallyDisabled = YES;
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
  if (!self.started) {
    return;
  }

  self.started = NO;
  // Leads to `containedPresenterDidDismiss`. Either reference self in
  // `containedPresenterDidDismiss` or before this line.
  [self.presenter dismissAnimated:animated];
}

@end
