// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_coordinator.h"

#import "base/mac/foundation_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/handoff_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrivacyCoordinator () <
    ClearBrowsingDataUIDelegate,
    PrivacyNavigationCommands,
    PrivacyTableViewControllerPresentationDelegate>

@property(nonatomic, strong) id<ApplicationCommands> handler;
@property(nonatomic, strong) PrivacyTableViewController* viewController;

@end

@implementation PrivacyCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ([super initWithBaseViewController:navigationController browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.handler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                    ApplicationCommands);

  ReauthenticationModule* module = nil;
  if (base::FeatureList::IsEnabled(kIncognitoAuthentication)) {
    module = [[ReauthenticationModule alloc] init];
  }
  self.viewController =
      [[PrivacyTableViewController alloc] initWithBrowser:self.browser
                                   reauthenticationModule:module];

  DCHECK(self.baseNavigationController);
  self.viewController.handler = self;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  self.viewController.presentationDelegate = self;
  self.viewController.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());
}

- (void)stop {
  self.viewController = nil;
}

#pragma mark - PrivacyTableViewControllerPresentationDelegate

- (void)privacyTableViewControllerWasRemoved:
    (PrivacyTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate privacyCoordinatorViewControllerWasRemoved:self];
}

#pragma mark - PrivacyNavigationCommands

- (void)showHandoff {
  HandoffTableViewController* viewController =
      [[HandoffTableViewController alloc]
          initWithBrowserState:self.browser->GetBrowserState()];
  viewController.dispatcher = self.viewController.dispatcher;
  [self.baseNavigationController pushViewController:viewController
                                           animated:YES];
}

- (void)showClearBrowsingData {
  ClearBrowsingDataTableViewController* viewController =
      [[ClearBrowsingDataTableViewController alloc]
          initWithBrowser:self.browser];
  viewController.dispatcher = self.viewController.dispatcher;
  viewController.delegate = self;
  [self.baseNavigationController pushViewController:viewController
                                           animated:YES];
}

#pragma mark - ClearBrowsingDataUIDelegate

- (void)openURL:(const GURL&)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.handler closeSettingsUIAndOpenURL:command];
}

- (void)dismissClearBrowsingData {
  SettingsNavigationController* navigationController =
      base::mac::ObjCCastStrict<SettingsNavigationController>(
          self.viewController.navigationController);
  [navigationController closeSettings];
}
@end
