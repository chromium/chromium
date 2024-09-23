// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
@interface ClearBrowsingDataCoordinator () <BrowserObserving,
                                            ClearBrowsingDataUIDelegate> {
  // Observe BrowserObserver to prevent any access to Browser after its
  // destroyed.
  std::unique_ptr<BrowserObserverBridge> _browserObserver;
}

@property(nonatomic, strong) id<ApplicationCommands> handler;
@property(nonatomic, strong)
    ClearBrowsingDataTableViewController* viewController;

@end

@implementation ClearBrowsingDataCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(!_browserObserver);
  _browserObserver =
      std::make_unique<BrowserObserverBridge>(self.browser, self);

  self.handler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                    ApplicationCommands);

  self.viewController = [[ClearBrowsingDataTableViewController alloc]
      initWithBrowser:self.browser];
  self.viewController.dispatcher = static_cast<id<ApplicationCommands>>(
      self.browser->GetCommandDispatcher());

  self.viewController.delegate = self;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [self.viewController prepareForDismissal];
  self.viewController.delegate = nil;
  self.viewController.dispatcher = nil;
  [self.viewController stop];
  self.viewController = nil;
  _browserObserver.reset();

  [super stop];
}

#pragma mark - ClearBrowsingDataUIDelegate

- (void)clearBrowsingDataTableViewController:
            (ClearBrowsingDataTableViewController*)controller
                              wantsToOpenURL:(const GURL&)URL {
  CHECK_EQ(self.viewController, controller);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.handler closeSettingsUIAndOpenURL:command];
}

- (void)clearBrowsingDataTableViewControllerWantsDismissal:
    (ClearBrowsingDataTableViewController*)controller {
  SettingsNavigationController* navigationController =
      base::apple::ObjCCastStrict<SettingsNavigationController>(
          self.viewController.navigationController);
  CHECK_EQ(controller, self.viewController);
  // The user tapped the "done" button, so the entire settings should be
  // dismissed, not only the CBD. It is thus sufficient to dismiss the
  // navigationController, which will be in charge of dismissing every view it
  // owns, including the current coordinator. Hence, there is no need to send a
  // message to the delegate requesting to stop `self`.
  [navigationController closeSettings];
}

- (void)clearBrowsingDataTableViewControllerWasRemoved:
    (ClearBrowsingDataTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate clearBrowsingDataCoordinatorViewControllerWasRemoved:self];
}

#pragma mark - BrowserObserving

- (void)browserDestroyed:(Browser*)browser {
  DCHECK_EQ(browser, self.browser);
  [self stop];
}

@end
