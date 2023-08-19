// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/handoff_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

@interface PrivacyCoordinator () <
    ClearBrowsingDataCoordinatorDelegate,
    PrivacyNavigationCommands,
    PrivacySafeBrowsingCoordinatorDelegate,
    PrivacyTableViewControllerPresentationDelegate,
    LockdownModeCoordinatorDelegate>

@property(nonatomic, strong) id<ApplicationCommands> handler;
@property(nonatomic, strong) PrivacyTableViewController* viewController;
// Coordinator for Privacy Safe Browsing settings.
@property(nonatomic, strong)
    PrivacySafeBrowsingCoordinator* safeBrowsingCoordinator;

// The coordinator for the clear browsing data screen.
@property(nonatomic, strong)
    ClearBrowsingDataCoordinator* clearBrowsingDataCoordinator;

// Coordinator for Lockdown Mode settings.
@property(nonatomic, strong) LockdownModeCoordinator* lockdownModeCoordinator;

@end

@implementation PrivacyCoordinator

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

  ReauthenticationModule* module = [[ReauthenticationModule alloc] init];
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
  [self.clearBrowsingDataCoordinator stop];
  self.clearBrowsingDataCoordinator = nil;
  [self stopLockdownModeCoordinator];
  [self stopSafeBrowsingCoordinator];

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
  self.clearBrowsingDataCoordinator = [[ClearBrowsingDataCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.clearBrowsingDataCoordinator.delegate = self;
  [self.clearBrowsingDataCoordinator start];
}

- (void)showSafeBrowsing {
  DCHECK(!self.safeBrowsingCoordinator);
  self.safeBrowsingCoordinator = [[PrivacySafeBrowsingCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.safeBrowsingCoordinator.delegate = self;
  [self.safeBrowsingCoordinator start];
}

- (void)showLockdownMode {
  DCHECK(!self.lockdownModeCoordinator);
  self.lockdownModeCoordinator = [[LockdownModeCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.lockdownModeCoordinator.delegate = self;
  [self.lockdownModeCoordinator start];
}

#pragma mark - ClearBrowsingDataCoordinatorDelegate

- (void)clearBrowsingDataCoordinatorViewControllerWasRemoved:
    (ClearBrowsingDataCoordinator*)coordinator {
  DCHECK_EQ(self.clearBrowsingDataCoordinator, coordinator);
  [self.clearBrowsingDataCoordinator stop];
  self.clearBrowsingDataCoordinator = nil;
}

#pragma mark - SafeBrowsingCoordinatorDelegate

- (void)privacySafeBrowsingCoordinatorDidRemove:
    (PrivacySafeBrowsingCoordinator*)coordinator {
  DCHECK_EQ(self.safeBrowsingCoordinator, coordinator);
  [self stopSafeBrowsingCoordinator];
}

#pragma mark - LockdownModeCoordinatorDelegate

- (void)lockdownModeCoordinatorDidRemove:(LockdownModeCoordinator*)coordinator {
  DCHECK_EQ(self.lockdownModeCoordinator, coordinator);
  [self stopLockdownModeCoordinator];
}

#pragma mark - Private

- (void)stopLockdownModeCoordinator {
  [self.lockdownModeCoordinator stop];
  self.lockdownModeCoordinator.delegate = nil;
  self.lockdownModeCoordinator = nil;
}

#pragma mark - Private

- (void)stopSafeBrowsingCoordinator {
  [self.safeBrowsingCoordinator stop];
  self.safeBrowsingCoordinator.delegate = nil;
  self.safeBrowsingCoordinator = nil;
}

@end
