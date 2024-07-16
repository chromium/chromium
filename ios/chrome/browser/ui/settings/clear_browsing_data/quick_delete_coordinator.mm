// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_coordinator.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_mediator.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_presentation_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_view_controller.h"

@interface QuickDeleteCoordinator () <QuickDeleteBrowsingDataDelegate,
                                      QuickDeletePresentationCommands,
                                      UIAdaptivePresentationControllerDelegate>
@end

@implementation QuickDeleteCoordinator {
  QuickDeleteViewController* _viewController;
  QuickDeleteMediator* _mediator;
  QuickDeleteBrowsingDataCoordinator* _browsingDataCoordinator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  CHECK(!browserState->IsOffTheRecord());

  BrowsingDataCounterWrapperProducer* producer =
      [[BrowsingDataCounterWrapperProducer alloc]
          initWithBrowserState:browserState];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForBrowserState(browserState);
  DiscoverFeedService* discoverFeedService =
      DiscoverFeedServiceFactory::GetForBrowserState(browserState);

  _mediator =
      [[QuickDeleteMediator alloc] initWithPrefs:browserState->GetPrefs()
              browsingDataCounterWrapperProducer:producer
                                 identityManager:identityManager
                             browsingDataRemover:browsingDataRemover
                             discoverFeedService:discoverFeedService];
  _mediator.presentationHandler = self;

  _viewController = [[QuickDeleteViewController alloc] init];
  _mediator.consumer = _viewController;

  _viewController.presentationHandler = self;
  _viewController.mutator = _mediator;
  _viewController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  [self disconnect];
}

#pragma mark - QuickDeletePresentationCommands

- (void)dismissQuickDelete {
  id<QuickDeleteCommands> quickDeleteHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), QuickDeleteCommands);
  [quickDeleteHandler stopQuickDelete];
}

- (void)openMyActivityURL:(const GURL&)URL {
  if (URL == GURL(kClearBrowsingDataDSESearchUrlInFooterURL)) {
    base::UmaHistogramEnumeration("Settings.ClearBrowsingData.OpenMyActivity",
                                  MyActivityNavigation::kSearchHistory);
  } else if (URL == GURL(kClearBrowsingDataDSEMyActivityUrlInFooterURL)) {
    base::UmaHistogramEnumeration("Settings.ClearBrowsingData.OpenMyActivity",
                                  MyActivityNavigation::kTopLevel);
  } else {
    NOTREACHED_NORETURN();
  }

  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [handler closeSettingsUIAndOpenURL:command];
}

- (void)showBrowsingDataPage {
  [_browsingDataCoordinator stop];

  QuickDeleteBrowsingDataCoordinator* browsingDataCoordinator =
      [[QuickDeleteBrowsingDataCoordinator alloc]
          initWithBaseViewController:_viewController
                             browser:self.browser];
  _browsingDataCoordinator = browsingDataCoordinator;
  [_browsingDataCoordinator start];
  _browsingDataCoordinator.delegate = self;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self disconnect];
  [self dismissQuickDelete];
}

#pragma mark - QuickDeleteBrowsingDataDelegate

- (void)stopBrowsingDataPage {
  [_browsingDataCoordinator stop];
  _browsingDataCoordinator = nil;
}

#pragma mark - Private

// Disconnects all instances.
- (void)disconnect {
  _viewController.presentationHandler = nil;
  _viewController.mutator = nil;
  _viewController.presentationController.delegate = nil;
  _viewController = nil;

  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;

  _browsingDataCoordinator.delegate = nil;
  [_browsingDataCoordinator stop];
  _browsingDataCoordinator = nil;
}

@end
