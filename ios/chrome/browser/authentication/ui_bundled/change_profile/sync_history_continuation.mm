// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/sync_history_continuation.h"

#import "base/functional/callback_helpers.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace {

// Implementation of the continuation opening the history opt-in view.
void ChangeProfileSyncHistoryContinuation(
    signin_metrics::AccessPoint accessPoint,
    BOOL optionalHistorySync,
    SceneState* scene_state,
    base::OnceClosure closure) {
  Browser* browser =
      scene_state.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  // Check that there is a signed in account.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser->GetProfile());
  CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);

  // kHistorySync triggers the history sync opt-in. The user must be already
  // signed in.
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kHistorySync
               identity:nil
            accessPoint:accessPoint
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
             completion:nil];
  command.optionalHistorySync = optionalHistorySync;

  [applicationHandler showSignin:command baseViewController:nil];

  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileSyncHistoryContinuation(
    signin_metrics::AccessPoint accessPoint,
    BOOL optionalHistorySync) {
  return base::BindOnce(&ChangeProfileSyncHistoryContinuation, accessPoint,
                        optionalHistorySync);
}
