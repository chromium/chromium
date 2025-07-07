// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/identity_snackbar/utils.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

// Displays the identity confirmation snackbar with `identity`.
void TriggerAccountSwitchSnackbarWithIdentity(id<SystemIdentity> identity,
                                              Browser* browser) {
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
  UIImage* avatar = ChromeAccountManagerServiceFactory::GetForProfile(profile)
                        ->GetIdentityAvatarWithIdentity(
                            identity, IdentityAvatarSize::Regular);
  ManagementState management_state =
      GetManagementState(IdentityManagerFactory::GetForProfile(profile),
                         AuthenticationServiceFactory::GetForProfile(profile),
                         profile->GetPrefs());
  MDCSnackbarMessage* snackbar_title =
      [[IdentitySnackbarMessage alloc] initWithName:identity.userGivenName
                                              email:identity.userEmail
                                             avatar:avatar
                                    managementState:management_state];
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbar_commands_handler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbar_commands_handler
      showSnackbarMessageOverBrowserToolbar:snackbar_title];
}
