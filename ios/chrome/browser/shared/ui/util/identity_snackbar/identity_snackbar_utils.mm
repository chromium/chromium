// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_utils.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Name of the histogram recording whether the identity snackbar had a name to
// display.
const char kIdentitySnackbarHadUserName[] =
    "Signin.IdentitySnackbarHadUserName";

// Point size for the icons.
constexpr CGFloat kSymbolsPointSize = 24;

// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kSymbolsPointSize));
#else
  return MakeSymbolMulticolor(
      DefaultSymbolWithPointSize(kGearshape2Symbol, kSymbolsPointSize));
#endif
}

// Returns a tinted version of the enterprise symbol.
UIImage* GetEnterpriseIcon() {
  UIColor* color = [UIColor colorNamed:kTextSecondaryColor];
  return SymbolWithPalette(
      CustomSymbolWithPointSize(kEnterpriseSymbol, kSymbolsPointSize),
      @[ color ]);
}

}  // namespace

SnackbarMessage* CreateIdentitySnackbarMessage(id<SystemIdentity> identity,
                                               Browser* browser) {
  CHECK(identity, base::NotFatalUntil::M151);
  // Retrieve necessary services and profile information.
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  ManagementState management_state =
      GetManagementState(identity_manager, auth_service, profile->GetPrefs());

  base::UmaHistogramBoolean(
      /*name=*/kIdentitySnackbarHadUserName,
      /*sample=*/(identity.userGivenName != nil));

  // Determine the main title of the snackbar.
  NSString* title =
      (identity.userGivenName)
          ? l10n_util::GetNSStringF(
                IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
                base::SysNSStringToUTF16(identity.userGivenName))
          : l10n_util::GetNSString(IDS_IOS_SIGNIN_ACCOUNT_NOTIFICATION_TITLE);

  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];

  // Configure subtitles based on management state and screen width.
  if (management_state.is_managed()) {
    // On compact screens, use two separate lines for email and management
    // status.
    if (IsCompactWidth(browser->GetSceneState().window.rootViewController)) {
      message.subtitle = identity.userEmail;
      if (AreSeparateProfilesForManagedAccountsEnabled()) {
        message.secondarySubtitle =
            l10n_util::GetNSString(management_state.is_browser_managed()
                                       ? IDS_IOS_ENTERPRISE_BROWSER_MANAGED
                                       : IDS_IOS_ENTERPRISE_ACCOUNT_MANAGED);
      } else {
        message.secondarySubtitle = l10n_util::GetNSString(
            IDS_IOS_ENTERPRISE_MANAGED_BY_YOUR_ORGANIZATION);
      }
    } else {
      // On regular screens, combine email and management status into one line.
      if (management_state.is_browser_managed()) {
        message.subtitle = l10n_util::GetNSStringF(
            IDS_IOS_ENTERPRISE_SWITCH_TO_MANAGED_BROWSER_WIDE_SCREEN,
            base::SysNSStringToUTF16(identity.userEmail));
      } else if (AreSeparateProfilesForManagedAccountsEnabled()) {
        message.subtitle = l10n_util::GetNSStringF(
            IDS_IOS_ENTERPRISE_SWITCH_TO_MANAGED_ACCOUNT_WIDE_SCREEN,
            base::SysNSStringToUTF16(identity.userEmail));
      } else {
        message.subtitle = l10n_util::GetNSStringF(
            IDS_IOS_ENTERPRISE_SWITCH_TO_MANAGED_WIDE_SCREEN,
            base::SysNSStringToUTF16(identity.userEmail));
      }
    }
  } else {
    // For unmanaged accounts, only show the email.
    message.subtitle = identity.userEmail;
  }

  // Configure accessory views.
  message.leadingAccessoryImage =
      GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
          identity, IdentityAvatarSize::Regular);
  message.roundLeadingAccessoryView = YES;

  message.trailingAccessoryImage = management_state.is_managed()
                                       ? GetEnterpriseIcon()
                                       : GetBrandedGoogleServicesSymbol();
  message.roundTrailingAccessoryView = YES;

  return message;
}

void TriggerAccountSwitchSnackbarWithIdentity(id<SystemIdentity> identity,
                                              Browser* browser) {
  SnackbarMessage* message = CreateIdentitySnackbarMessage(identity, browser);
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbar_commands_handler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbar_commands_handler showSnackbarMessageOverBrowserToolbar:message];
}
