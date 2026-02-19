// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_chrome_passkey_client.h"

#import "base/apple/foundation_util.h"
#import "base/containers/span.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/webauthn/ios/features.h"
#import "components/webauthn/ios/ios_passkey_client_commands.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webauthn/public/scoped_passkey_keychain_provider_override.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"
#import "ios/web/public/web_state.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import "ios/chrome/browser/credential_provider/model/credential_provider_browser_agent.h"
#endif

// Helper class to act as a delegate for the PasskeyKeychainProviderBridge.
@interface IOSChromePasskeyClientBridgeDelegate
    : NSObject <PasskeyKeychainProviderBridgeDelegate>

- (instancetype)initWithClient:(IOSChromePasskeyClient*)client;

@end

@implementation IOSChromePasskeyClientBridgeDelegate {
  raw_ptr<IOSChromePasskeyClient> _client;
}

- (instancetype)initWithClient:(IOSChromePasskeyClient*)client {
  self = [super init];
  if (self) {
    _client = client;
  }
  return self;
}

- (void)performUserVerificationIfNeeded:(ProceduralBlock)completion {
  // TODO(crbug.com/460485614): Implement user verification.
  completion();
}

- (void)showWelcomeScreenWithPurpose:
            (webauthn::PasskeyWelcomeScreenPurpose)purpose
                          completion:
                              (webauthn::PasskeyWelcomeScreenAction)completion {
  [_client->GetCommandHandler() showPasskeyWelcomeScreenForPurpose:purpose
                                                        completion:completion];
}

- (void)providerDidCompleteReauthentication {
  // TODO(crbug.com/460485614): Handle that.
}

@end

IOSChromePasskeyClient::IOSChromePasskeyClient(web::WebState* web_state) {
  CHECK(web_state);
  web_state_ = web_state->GetWeakPtr();
  profile_ = ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
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
        GetApplicationContext()->GetLocalState()->GetBoolean(
            metrics::prefs::kMetricsReportingEnabled);
    passkey_keychain_provider_bridge_ = [[PasskeyKeychainProviderBridge alloc]
          initWithEnableLogging:metrics_reporting_enabled
        navigationItemTitleView:nil];
  }

  bridge_delegate_ =
      [[IOSChromePasskeyClientBridgeDelegate alloc] initWithClient:this];
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

bool IOSChromePasskeyClient::PerformUserVerification() {
  // TODO(crbug.com/460484682): Perform user verification.
  // See PasskeyKeychainProvider::Reauthenticate and ReauthenticationModule.
  return false;
}

void IOSChromePasskeyClient::FetchKeys(webauthn::ReauthenticatePurpose purpose,
                                       webauthn::KeysFetchedCallback callback) {
  CoreAccountInfo account =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  auto completion_block = base::CallbackToBlock(base::BindOnce(
      [](id<IOSPasskeyClientCommands> handler,
         webauthn::KeysFetchedCallback inner_callback,
         webauthn::SharedKeyList trusted_vault_keys, NSError* error) {
        std::move(inner_callback).Run(std::move(trusted_vault_keys), error);
        [handler dismissPasskeyWelcomeScreen];
      },
      command_handler_, std::move(callback)));
  [GetPasskeyKeychainProviderBridge()
      fetchTrustedVaultKeysForGaia:account.gaia.ToNSString()
                        credential:nil
                           purpose:purpose
                        completion:completion_block];
}

void IOSChromePasskeyClient::ShowSuggestionBottomSheet(
    RequestInfo request_info) {
  [command_handler_ showPasskeySuggestionBottomSheet:request_info];
}

void IOSChromePasskeyClient::ShowCreationBottomSheet(RequestInfo request_info) {
  [command_handler_ showPasskeyCreationBottomSheet:request_info.request_id];
}

void IOSChromePasskeyClient::ShowInterstitial(InterstitialCallback callback) {
  // TODO(crbug.com/479583177): Implement incognito interstitial UI.
  std::move(callback).Run(true);
}

void IOSChromePasskeyClient::AllowPasskeyCreationInfobar(bool allowed) {
#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  if (!web_state_) {
    return;
  }

  BrowserList* browserList = BrowserListFactory::GetForProfile(profile_);
  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kAll)) {
    if (auto* agent = CredentialProviderBrowserAgent::FromBrowser(browser)) {
      agent->SetInfobarAllowed(allowed);
    }
  }
#endif  // BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
}

password_manager::WebAuthnCredentialsDelegate*
IOSChromePasskeyClient::GetWebAuthnCredentialsDelegateForDriver(
    IOSPasswordManagerDriver* driver) {
  PasswordTabHelper* password_tab_helper =
      PasswordTabHelper::FromWebState(web_state_.get());
  if (!password_tab_helper) {
    return nullptr;
  }

  password_manager::PasswordManagerClient* client =
      password_tab_helper->GetPasswordManagerClient();
  CHECK(client);
  return client->GetWebAuthnCredentialsDelegateForDriver(driver);
}
