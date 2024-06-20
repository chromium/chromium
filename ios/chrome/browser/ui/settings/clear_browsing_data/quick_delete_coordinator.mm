// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_mediator.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_presentation_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_view_controller.h"

@interface QuickDeleteCoordinator () <QuickDeletePresentationCommands>
@end

@implementation QuickDeleteCoordinator {
  QuickDeleteViewController* _viewController;
  QuickDeleteMediator* _mediator;
}

#pragma mark - ChromeCoordinator
- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  _mediator =
      [[QuickDeleteMediator alloc] initWithPrefs:browserState->GetPrefs()
              browsingDataCounterWrapperProducer:
                  [[BrowsingDataCounterWrapperProducer alloc]
                      initWithBrowserState:browserState]];

  _viewController = [[QuickDeleteViewController alloc] init];
  _mediator.consumer = _viewController;

  _viewController.presentationHandler = self;
  _viewController.mutator = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController.presentationHandler = nil;
  _viewController.mutator = nil;
  _viewController = nil;

  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;
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

@end
