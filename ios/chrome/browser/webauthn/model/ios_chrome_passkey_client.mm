// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_chrome_passkey_client.h"

#import <utility>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/containers/span.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_reporting_choice_service.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_service.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_types.h"
#import "components/webauthn/ios/features.h"
#import "components/webauthn/ios/ios_passkey_client.h"
#import "components/webauthn/ios/ios_passkey_client_commands.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_device_authorization_service_factory.h"
#import "ios/chrome/browser/webauthn/public/scoped_passkey_keychain_provider_override.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import "ios/chrome/browser/credential_provider/model/ios_credential_provider_infobar_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#endif

namespace {

// Returns the signed-in primary account info for `profile`, or an empty
// `CoreAccountInfo` if unavailable.
CoreAccountInfo GetPrimaryAccountInfo(ProfileIOS* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return CoreAccountInfo();
  }
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
}

}  // namespace

// Helper class to act as a delegate for the PasskeyKeychainProviderBridge.
@interface IOSChromePasskeyClientBridgeDelegate
    : NSObject <PasskeyKeychainProviderBridgeDelegate>

- (instancetype)initWithClient:(base::WeakPtr<IOSChromePasskeyClient>)client
                  reauthModule:(id<ReauthenticationProtocol>)reauthModule;

- (void)setUserVerificationStatus:
    (webauthn::PasskeyUserVerificationStatus)status;

- (webauthn::PasskeyUserVerificationStatus)userVerificationStatus;

@end

@implementation IOSChromePasskeyClientBridgeDelegate {
  // Weak reference because Objective-C retain cycles or autorelease pools (for
  // example in tests) may outlive `IOSChromePasskeyClient`.
  base::WeakPtr<IOSChromePasskeyClient> _client;
  __weak id<ReauthenticationProtocol> _reauthModule;
  webauthn::PasskeyUserVerificationStatus _userVerificationStatus;
}

- (instancetype)initWithClient:(base::WeakPtr<IOSChromePasskeyClient>)client
                  reauthModule:(id<ReauthenticationProtocol>)reauthModule {
  self = [super init];
  if (self) {
    CHECK(client);
    _client = std::move(client);
    _reauthModule = reauthModule;
  }
  return self;
}

- (void)setUserVerificationStatus:
    (webauthn::PasskeyUserVerificationStatus)status {
  _userVerificationStatus = status;
  if (status == webauthn::PasskeyUserVerificationStatus::kRequired) {
    [_reauthModule clearAuthValidity];
  }
}

- (webauthn::PasskeyUserVerificationStatus)userVerificationStatus {
  return _userVerificationStatus;
}

- (void)performUserVerificationIfNeeded:
    (UserVerificationCompletionBlock)completion {
  if (_userVerificationStatus !=
      webauthn::PasskeyUserVerificationStatus::kRequired) {
    completion(YES);
    return;
  }

  if (![_reauthModule canAttemptReauth]) {
    completion(NO);
    return;
  }
  [_reauthModule
      attemptReauthWithLocalizedReason:
          l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_START_REAUTH_REASON)
                  canReusePreviousAuth:YES
                               handler:^(ReauthenticationResult result) {
                                 completion(result !=
                                            ReauthenticationResult::kFailure);
                               }];
}

- (void)showWelcomeScreenWithPurpose:
            (webauthn::PasskeyWelcomeScreenPurpose)purpose
                          completion:
                              (webauthn::PasskeyWelcomeScreenAction)completion {
  if (!_client) {
    return;
  }
  [_client->GetCommandHandler() showPasskeyWelcomeScreenForPurpose:purpose
                                                        completion:completion];
}

- (void)providerDidCompleteReauthentication {
  _userVerificationStatus = webauthn::PasskeyUserVerificationStatus::kCompleted;
}

@end

IOSChromePasskeyClient::IOSChromePasskeyClient(web::WebState* web_state) {
  CHECK(web_state);
  web_state_ = web_state->GetWeakPtr();

  // IOSChromePasskeyClient should behave exactly the same way in and out of
  // incognito. In incognito mode, an interstitial is presented to the user
  // before allowing assertion or registration requests, but the logic should
  // always use the original profile.
  profile_ = ProfileIOS::FromBrowserState(web_state_->GetBrowserState())
                 ->GetOriginalProfile();
}

IOSChromePasskeyClient::~IOSChromePasskeyClient() = default;

PasskeyKeychainProviderBridge*
IOSChromePasskeyClient::GetPasskeyKeychainProviderBridge() {
  if (passkey_keychain_provider_bridge_) {
    return passkey_keychain_provider_bridge_;
  }

  PasskeyKeychainProviderBridge* test_bridge =
      ScopedPasskeyKeychainProviderBridgeOverride::Get();
  if (test_bridge) {
    passkey_keychain_provider_bridge_ = test_bridge;
  } else {
    bool metrics_reporting_enabled =
        metrics::MetricsReportingChoiceService::IsBasicMetricsReportingEnabled(
            GetApplicationContext()->GetLocalState());
    passkey_keychain_provider_bridge_ = [[PasskeyKeychainProviderBridge alloc]
          initWithEnableLogging:metrics_reporting_enabled
        navigationItemTitleView:nil];
  }

  id<ReauthenticationProtocol> reauthModule =
      ReauthenticationServiceFactory::GetForProfile(profile_)
          ->GetReauthModule();
  bridge_delegate_ = [[IOSChromePasskeyClientBridgeDelegate alloc]
      initWithClient:weak_ptr_factory_.GetWeakPtr()
        reauthModule:reauthModule];
  passkey_keychain_provider_bridge_.delegate = bridge_delegate_;

  return passkey_keychain_provider_bridge_;
}

void IOSChromePasskeyClient::SetIOSPasskeyClientCommandsHandler(
    id<IOSPasskeyClientCommands> handler) {
  command_handler_ = handler;
}

id<IOSPasskeyClientCommands> IOSChromePasskeyClient::GetCommandHandler() const {
  return command_handler_;
}

void IOSChromePasskeyClient::FetchTrustedVaultKeys(
    webauthn::ReauthenticatePurpose purpose,
    webauthn::PasskeyUserVerificationStatus user_verification_status,
    bool allow_ui,
    webauthn::FetchTrustedVaultKeysCallback callback) {
  if (!allow_ui) {
    CHECK_NE(user_verification_status,
             webauthn::PasskeyUserVerificationStatus::kRequired);
  }

  IOSChromePasskeyClientBridgeDelegate* delegate =
      base::apple::ObjCCast<IOSChromePasskeyClientBridgeDelegate>(
          GetPasskeyKeychainProviderBridge().delegate);

  [delegate setUserVerificationStatus:user_verification_status];

  CoreAccountInfo account = GetPrimaryAccountInfo(profile_);

  auto completion_block = base::CallbackToBlock(base::BindOnce(
      [](id<IOSPasskeyClientCommands> handler,
         IOSChromePasskeyClientBridgeDelegate* delegate, bool allow_ui,
         webauthn::FetchTrustedVaultKeysCallback inner_callback,
         webauthn::SharedKeyList trusted_vault_keys, NSError* error) {
        bool did_complete_uv =
            [delegate userVerificationStatus] ==
            webauthn::PasskeyUserVerificationStatus::kCompleted;
        std::move(inner_callback)
            .Run(std::move(trusted_vault_keys), did_complete_uv);
        if (allow_ui) {
          [handler dismissPasskeyWelcomeScreen];
        }
      },
      command_handler_, delegate, allow_ui, std::move(callback)));
  [GetPasskeyKeychainProviderBridge()
      fetchTrustedVaultKeysForGaia:account.gaia.ToNSString()
                        credential:nil
                           purpose:purpose
                           allowUI:allow_ui
                        completion:completion_block];
}

void IOSChromePasskeyClient::FetchDeviceAuthorizationKeys(
    bool allow_reauth,
    webauthn::FetchDeviceAuthKeysCallback callback) {
  webauthn::DeviceAuthorizationService* service =
      IOSDeviceAuthorizationServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run(webauthn::DeviceAuthFetchResult());
    return;
  }

  // TODO(crbug.com/586442316): Explore showing a spinner while fetching keys.
  service->GetOrFetchKeys(base::BindOnce(
      &IOSChromePasskeyClient::OnDeviceAuthorizationKeysFetched,
      weak_ptr_factory_.GetWeakPtr(), allow_reauth, std::move(callback)));
}

void IOSChromePasskeyClient::OnDeviceAuthorizationKeysFetched(
    bool allow_reauth,
    webauthn::FetchDeviceAuthKeysCallback callback,
    webauthn::DeviceAuthFetchResult result) {
  if (result.status() !=
      webauthn::DeviceAuthFetchResult::Status::kReAuthRequired) {
    std::move(callback).Run(std::move(result));
    return;
  }

  const webauthn::DeviceAuthorizationReAuthParams* reauth_params =
      result.reauth_params();
  if (!allow_reauth || !command_handler_ || !reauth_params ||
      reauth_params->plt().empty()) {
    std::move(callback).Run(webauthn::DeviceAuthFetchResult());
    return;
  }

  NSString* packaged_lookup_token =
      base::SysUTF8ToNSString(reauth_params->plt());
  EmbeddedReAuthCallback reauth_callback =
      base::BindOnce(&IOSChromePasskeyClient::OnEmbeddedReAuthCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  [command_handler_
      showPasskeyEmbeddedReAuthWithPackagedLookupToken:packaged_lookup_token
                                              callback:std::move(
                                                           reauth_callback)];
}

void IOSChromePasskeyClient::OnEmbeddedReAuthCompleted(
    webauthn::FetchDeviceAuthKeysCallback callback,
    std::string reauth_proof_token) {
  webauthn::DeviceAuthorizationService* service =
      IOSDeviceAuthorizationServiceFactory::GetForProfile(profile_);
  if (!service || reauth_proof_token.empty()) {
    std::move(callback).Run(webauthn::DeviceAuthFetchResult());
    return;
  }

  service->FetchKeysWithReAuthToken(std::move(reauth_proof_token),
                                    std::move(callback));
}

void IOSChromePasskeyClient::ShowSuggestionBottomSheet(
    RequestInfo request_info) {
  [command_handler_ showPasskeySuggestionBottomSheet:request_info];
}

void IOSChromePasskeyClient::ShowCreationBottomSheet(RequestInfo request_info) {
  [command_handler_ showPasskeyCreationBottomSheet:std::move(request_info)];
}

void IOSChromePasskeyClient::ShowInterstitial(InterstitialCallback callback) {
  if (!command_handler_) {
    std::move(callback).Run(false);
    return;
  }

  [command_handler_ showPasskeyIncognitoInterstitial:std::move(callback)];
}

void IOSChromePasskeyClient::ShowPasskeyCreationInfobar(
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  // `web_state_` may have been destroyed while async passkey creation was in
  // flight, or backgrounded before completion.
  if (!web_state_ || !web_state_->IsVisible()) {
    return;
  }

  // The primary account may have signed out while key fetching was in flight.
  CoreAccountInfo account = GetPrimaryAccountInfo(profile_);
  if (account.email.empty()) {
    return;
  }

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
  for (Browser* browser : browser_list->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndIncognito)) {
    if (browser->GetWebStateList()->GetIndexOfWebState(web_state_.get()) !=
        WebStateList::kInvalidIndex) {
      id<SettingsCommands> settings_handler =
          HandlerForProtocol(browser->GetCommandDispatcher(), SettingsCommands);
      IOSCredentialProviderInfoBarDelegate::DisplayForWebState(
          web_state_.get(), std::move(account.email), passkey,
          settings_handler);
      return;
    }
  }
#endif  // BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
}

void IOSChromePasskeyClient::CancelPasskeyRequest(RequestInfo request_info) {
  [command_handler_ cancelPasskeyRequest:std::move(request_info)];
}

bool IOSChromePasskeyClient::IsGpmPasskeySavingEnabled() const {
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->GetBoolean(password_manager::prefs::kCredentialsEnableService) ||
      !prefs->GetBoolean(password_manager::prefs::kCredentialsEnablePasskeys)) {
    return false;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (!sync_service || !sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kPasswords)) {
    return false;
  }
  return true;
}

bool IOSChromePasskeyClient::IsAutomaticPasskeyUpgradeEnabled() const {
  return profile_->GetPrefs()->GetBoolean(
      password_manager::prefs::kAutomaticPasskeyUpgrades);
}

bool IOSChromePasskeyClient::IsBiometricsEnabled() const {
  id<ReauthenticationProtocol> reauth_module =
      ReauthenticationServiceFactory::GetForProfile(profile_)
          ->GetReauthModule();
  return [reauth_module canAttemptReauthWithBiometrics];
}

void IOSChromePasskeyClient::OnPasskeyCreated() {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (!password_manager_util::IsCredentialProviderEnabledOnStartup(
          localState)) {
    [command_handler_ showCredentialProviderPromoOnPasskeyCreated];
  }
}
