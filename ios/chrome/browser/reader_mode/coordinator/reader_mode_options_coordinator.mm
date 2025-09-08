// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_coordinator.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_mediator.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_controls_view.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"

namespace {

// The identifier for the custom content detent.
NSString* const kReaderModeOptionsViewControllerCustomDetentIdentifier =
    @"kReaderModeOptionsViewControllerCustomDetentIdentifier";

}  // namespace

@interface ReaderModeOptionsCoordinator () <
    UIAdaptivePresentationControllerDelegate>

@end

@implementation ReaderModeOptionsCoordinator {
  ReaderModeOptionsMediator* _mediator;
  ReaderModeOptionsViewController* _viewController;
  UINavigationController* _navigationController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIPHReaderModeOptionsUsed);

  _viewController = [[ReaderModeOptionsViewController alloc] init];
  _viewController.readerModeOptionsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ReaderModeOptionsCommands);
  DistillerService* distillerService =
      DistillerServiceFactory::GetForProfile(self.browser->GetProfile());
  _mediator = [[ReaderModeOptionsMediator alloc]
      initWithDistilledPagePrefs:distillerService->GetDistilledPagePrefs()
                    webStateList:self.browser->GetWebStateList()];
  _mediator.readerModeHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ReaderModeCommands);
  _viewController.mutator = _mediator;
  _viewController.controlsView.mutator = _mediator;
  _mediator.consumer = _viewController.controlsView;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  if (self.browser->GetProfile()->IsOffTheRecord()) {
    _navigationController.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }
  _navigationController.presentationController.delegate = self;
  // Initialize custom content detent.
  UISheetPresentationControllerDetent* contentDetent =
      [self createCustomContentDetent];
  _navigationController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;
  _navigationController.sheetPresentationController.detents =
      @[ contentDetent ];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // If the navigation controller is not dismissed programmatically i.e. not
  // dismissed using `dismissViewControllerAnimated:completion:`, then call
  // `-hideReaderModeOptions`.
  id<ReaderModeOptionsCommands> readerModeOptionsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ReaderModeOptionsCommands);
  [readerModeOptionsHandler hideReaderModeOptions];
}

#pragma mark - Private

// Returns the custom content detent.
- (UISheetPresentationControllerDetent*)createCustomContentDetent {
  __weak __typeof(self) weakSelf = self;
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:
          kReaderModeOptionsViewControllerCustomDetentIdentifier
                        resolver:^CGFloat(
                            id<UISheetPresentationControllerDetentResolutionContext>
                                context) {
                          return [weakSelf
                              resolveDetentValueForSheetPresentation:context];
                        }];
}

// Returns the appropriate detent value for a sheet presentation in `context`.
- (CGFloat)resolveDetentValueForSheetPresentation:
    (id<UISheetPresentationControllerDetentResolutionContext>)context {
  return [_viewController resolveDetentValueForSheetPresentation:context];
}

@end
