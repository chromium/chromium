// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_coordinator.h"

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ClearBrowsingDataCoordinator () <ClearBrowsingDataUIDelegate>

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
  self.handler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                    ApplicationCommands);

  self.viewController = [[ClearBrowsingDataTableViewController alloc]
      initWithBrowser:self.browser];
  self.viewController.dispatcher =
      static_cast<id<ApplicationCommands, BrowsingDataCommands>>(
          self.browser->GetCommandDispatcher());

  self.viewController.delegate = self;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [self.viewController prepareForDismissal];
  self.viewController.delegate = nil;
  self.viewController.dispatcher = nil;
  self.viewController = nil;
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

- (void)clearBrowsingDataTableViewControllerWasRemoved:
    (ClearBrowsingDataTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate clearBrowsingDataCoordinatorViewControllerWasRemoved:self];
}

@end
