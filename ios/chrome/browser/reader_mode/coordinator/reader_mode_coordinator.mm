// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_coordinator.h"

#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_mediator.h"
#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_coordinator.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"

@interface ReaderModeCoordinator () <ReaderModeOptionsCommands>
@end

@implementation ReaderModeCoordinator {
  ReaderModeViewController* _viewController;
  ReaderModeMediator* _mediator;
  ReaderModeOptionsCoordinator* _optionsCoordinator;
}

#pragma mark - Public

- (void)startAnimated:(BOOL)animated {
  _viewController = [[ReaderModeViewController alloc] init];
  _viewController.overscrollDelegate = self.overscrollDelegate;
  _viewController.delegate = self;
  ProfileIOS* profile = self.browser->GetProfile();
  BwgService* BWGService = BwgServiceFactory::GetForProfile(profile);
  DistillerService* distiller_service =
      DistillerServiceFactory::GetForProfile(self.browser->GetProfile());
  dom_distiller::DistilledPagePrefs* distilledPagePrefs =
      distiller_service ? distiller_service->GetDistilledPagePrefs() : nullptr;
  _mediator = [[ReaderModeMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
                BWGService:BWGService
        distilledPagePrefs:distilledPagePrefs];
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;
  [self.baseViewController addChildViewController:_viewController];
  [_viewController moveToParentViewController:self.baseViewController
                                     animated:animated];
  // Start handling Reader mode options commands.
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(ReaderModeOptionsCommands)];
}

- (void)stopAnimated:(BOOL)animated {
  // Stop handling Reader mode options commands.
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  // Ensure the options UI is dismissed.
  [self hideReaderModeOptions];
  // Disconnect mediator from model layer.
  [_mediator disconnect];
  _mediator = nil;
  // Dismiss Reader mode UI.
  [_viewController removeFromParentViewControllerAnimated:animated];
  _viewController = nil;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self startAnimated:NO];
}

- (void)stop {
  [self stopAnimated:NO];
}

#pragma mark - ReaderModeViewControllerDelegate

- (void)readerModeViewControllerAnimationDidComplete:
    (ReaderModeViewController*)controller {
  [self.delegate readerModeCoordinatorAnimationDidComplete:self];
}

#pragma mark - ReaderModeOptionsCommands

- (void)showReaderModeOptions {
  if ([_mediator BWGAvailableForProfile]) {
    id<PageActionMenuCommands> pageActionMenuHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), PageActionMenuCommands);
    // The flow when Page Action is available is to show the Page action menu.
    // The user will have to tap RM options button again from there.
    [pageActionMenuHandler showPageActionMenu];
    return;
  }
  if (_optionsCoordinator) {
    // If the Reader mode options UI is already presented then there is nothing
    // to do.
    return;
  }
  _optionsCoordinator = [[ReaderModeOptionsCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  [_optionsCoordinator start];
}

- (void)hideReaderModeOptions {
  if ([_mediator BWGAvailableForProfile]) {
    id<PageActionMenuCommands> pageActionMenuHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), PageActionMenuCommands);
    [pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
    return;
  }
  if (!_optionsCoordinator) {
    // If the Reader mode options UI is already dismissed then there is nothing
    // to do.
    return;
  }
  [_optionsCoordinator stop];
  _optionsCoordinator = nil;
}

@end
