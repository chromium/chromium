// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_settings_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_settings_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace {

// The URL for managing passes data in Google Wallet.
const char kWalletManagePassesDataURL[] =
    "https://wallet.google.com/wallet/settings/managepassesdata";

}  // namespace

@interface AutofillSettingsCoordinator () <
    AutofillSettingsMediatorDelegate,
    AutofillSettingsTableViewControllerDelegate>
@end

@implementation AutofillSettingsCoordinator {
  AutofillSettingsTableViewController* _viewController;
  AutofillSettingsMediator* _mediator;
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
  _viewController = [[AutofillSettingsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;

  ProfileIOS* originalProfile =
      self.browser->GetProfile()->GetOriginalProfile();
  id<ReauthenticationProtocol> reauthModule =
      ReauthenticationServiceFactory::GetForProfile(originalProfile)
          ->GetReauthModule();
  BOOL shouldShowWalletPromo = autofill::CanPerformAutofillAiAction(
      originalProfile, autofill::AutofillAiAction::kWalletDataSharingPromotion);

  _mediator = [[AutofillSettingsMediator alloc]
         initWithPrefService:originalProfile->GetPrefs()
             identityManager:IdentityManagerFactory::GetForProfile(
                                 originalProfile)
      reauthenticationModule:reauthModule
       shouldShowWalletPromo:shouldShowWalletPromo];

  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _viewController.mutator = _mediator;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - AutofillSettingsMediatorDelegate

- (void)autofillSettingsMediator:(AutofillSettingsMediator*)mediator
       didToggleEnhancedAutofill:(BOOL)enabled {
  autofill::SetEnhancedAutofillEnabled(
      self.browser->GetProfile()->GetOriginalProfile(), enabled);
}

#pragma mark - AutofillSettingsTableViewControllerDelegate

- (void)autofillSettingsTableViewControllerDidRemove:
    (AutofillSettingsTableViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate autofillSettingsCoordinatorDidRemove:self];
}

- (void)autofillSettingsTableViewControllerDidTapWalletPromoCard:
    (AutofillSettingsTableViewController*)controller {
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler
      closePresentedViewsAndOpenURL:
          [OpenNewTabCommand
              commandWithURLFromChrome:GURL(kWalletManagePassesDataURL)]];
}

@end
