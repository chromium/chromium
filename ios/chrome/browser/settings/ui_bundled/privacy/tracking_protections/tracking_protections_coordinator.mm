// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/tracking_protections/tracking_protections_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/tracking_protections/script_blocking/script_blocking_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/tracking_protections/tracking_protections_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@interface TrackingProtectionsCoordinator () <
    TrackingProtectionsViewControllerPresentationDelegate,
    ScriptBlockingCoordinatorDelegate>

@end

@implementation TrackingProtectionsCoordinator {
  // View controller presented by this coordinator.
  TrackingProtectionsViewController* _viewController;

  // Coordinator for the script blocking screen.
  ScriptBlockingCoordinator* _scriptBlockingCoordinator;
}

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

- (void)start {
  _viewController = [[TrackingProtectionsViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.presentationDelegate = self;
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _viewController.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;
  [self stopScriptBlockingCoordinator];
}

#pragma mark - TrackingProtectionsViewControllerPresentationDelegate

- (void)trackingProtectionsViewControllerDidRemove:
    (TrackingProtectionsViewController*)controller {
  [self.delegate trackingProtectionsCoordinatorDidRemove:self];
}

- (void)trackingProtectionsViewControllerSelectedScriptBlocking:
    (TrackingProtectionsViewController*)controller {
  _scriptBlockingCoordinator = [[ScriptBlockingCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  _scriptBlockingCoordinator.delegate = self;
  [_scriptBlockingCoordinator start];
}

#pragma mark - ScriptBlockingCoordinatorDelegate

- (void)scriptBlockingCoordinatorDidRemove:
    (ScriptBlockingCoordinator*)coordinator {
  [self stopScriptBlockingCoordinator];
}

#pragma mark - Private

- (void)stopScriptBlockingCoordinator {
  [_scriptBlockingCoordinator stop];
  _scriptBlockingCoordinator.delegate = nil;
  _scriptBlockingCoordinator = nil;
}

@end
