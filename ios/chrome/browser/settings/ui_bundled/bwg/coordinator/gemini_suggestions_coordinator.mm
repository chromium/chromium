// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_suggestions_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/settings/manage_sync/coordinator/manage_sync_settings_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_suggestions_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@interface GeminiSuggestionsCoordinator () <
    GeminiSettingsMediatorDelegate,
    GeminiSuggestionsViewControllerPresentationDelegate,
    ManageSyncSettingsCoordinatorDelegate>
@end

@implementation GeminiSuggestionsCoordinator {
  // View controller presented by this coordinator.
  GeminiSuggestionsViewController* _viewController;
  // Mediator used by this coordinator.
  GeminiSettingsMediator* _mediator;
  // Coordinator for the Manage Sync Settings table view.
  ManageSyncSettingsCoordinator* _manageSyncSettingsCoordinator;
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

#pragma mark - ChromeCoordinator

- (void)start {
  ProfileIOS* profile = self.profile;
  CommandDispatcher* commandDispatcher = self.browser->GetCommandDispatcher();
  _mediator = [[GeminiSettingsMediator alloc]
      initWithAuthService:AuthenticationServiceFactory::GetForProfile(profile)
              prefService:profile->GetPrefs()
          identityManager:IdentityManagerFactory::GetForProfile(profile)];
  _mediator.delegate = self;
  _mediator.sceneHandler = HandlerForProtocol(commandDispatcher, SceneCommands);

  _viewController = [[GeminiSuggestionsViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.mutator = _mediator;
  _viewController.presentationDelegate = self;
  _mediator.suggestionsConsumer = _viewController;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_manageSyncSettingsCoordinator stop];
  _manageSyncSettingsCoordinator = nil;
  _mediator.suggestionsConsumer = nil;
  [_mediator disconnect];
  _mediator = nil;
  _viewController.mutator = nil;
  _viewController.presentationDelegate = nil;
  _viewController = nil;
}

#pragma mark - GeminiSuggestionsViewControllerPresentationDelegate

- (void)geminiSuggestionsViewControllerWasRemoved:
    (GeminiSuggestionsViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate geminiSuggestionsCoordinatorViewControllerWasRemoved:self];
}

#pragma mark - GeminiSettingsMediatorDelegate

- (void)openSyncSettings {
  if (_manageSyncSettingsCoordinator) {
    return;
  }
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  if (!authService || !authService->HasPrimaryIdentity() ||
      !authService->SigninEnabled()) {
    return;
  }
  _manageSyncSettingsCoordinator = [[ManageSyncSettingsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  _manageSyncSettingsCoordinator.delegate = self;
  [_manageSyncSettingsCoordinator start];
}

#pragma mark - ManageSyncSettingsCoordinatorDelegate

- (void)manageSyncSettingsCoordinatorWasRemoved:
    (ManageSyncSettingsCoordinator*)coordinator {
  CHECK_EQ(_manageSyncSettingsCoordinator, coordinator);
  [_manageSyncSettingsCoordinator stop];
  _manageSyncSettingsCoordinator = nil;
}

@end
