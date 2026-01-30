// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_chrome_passkey_client.h"

#import "base/functional/callback.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/webauthn/ios/features.h"
#import "components/webauthn/ios/passkey_java_script_feature.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webauthn/model/scoped_passkey_keychain_provider_override.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider.h"
#import "ios/web/public/web_state.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import "ios/chrome/browser/credential_provider/model/credential_provider_browser_agent.h"
#endif

IOSChromePasskeyClient::IOSChromePasskeyClient(web::WebState* web_state) {
  CHECK(web_state);
  web_state_ = web_state->GetWeakPtr();
  profile_ = ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
}

IOSChromePasskeyClient::~IOSChromePasskeyClient() {}

PasskeyKeychainProvider* IOSChromePasskeyClient::GetPasskeyKeychainProvider() {
  // Returns the test provider if it's set, or the regular provider otherwise.
  PasskeyKeychainProvider* test_provider =
      ScopedPasskeyKeychainProviderOverride::Get();
  if (test_provider) {
    return test_provider;
  }

  if (!passkey_keychain_provider_) {
    bool metrics_reporting_enabled =
        GetApplicationContext()->GetLocalState()->GetBoolean(
            metrics::prefs::kMetricsReportingEnabled);
    passkey_keychain_provider_ =
        std::make_unique<PasskeyKeychainProvider>(metrics_reporting_enabled);
  }

  return passkey_keychain_provider_.get();
}

void IOSChromePasskeyClient::SetIOSPasskeyClientCommandsHandler(
    id<IOSPasskeyClientCommands> handler) {
  command_handler_ = handler;
}

bool IOSChromePasskeyClient::PerformUserVerification() {
  // TODO(crbug.com/460484682): Perform user verification.
  // See PasskeyKeychainProvider::Reauthenticate and ReauthenticationModule.
  return false;
}

void IOSChromePasskeyClient::FetchKeys(webauthn::ReauthenticatePurpose purpose,
                                       webauthn::KeysFetchedCallback callback) {
  // TODO(crbug.com/460485614): Allow onboarding, bootstrapping and reauth.
  CoreAccountInfo account =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  GetPasskeyKeychainProvider()->FetchKeys(account.gaia.ToNSString(), purpose,
                                          std::move(callback));
}

void IOSChromePasskeyClient::ShowSuggestionBottomSheet(
    RequestInfo request_info) {
  [command_handler_ showPasskeySuggestionBottomSheet:request_info.request_id];

  // TODO(crbug.com/460485496): remove the code below and related dependencies
  // once the bottom sheet is implemented.
  web::WebFramesManager* web_frames_manager =
      webauthn::PasskeyJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_.get());
  if (!web_frames_manager) {
    return;
  }

  web::WebFrame* web_frame =
      web_frames_manager->GetFrameWithId(request_info.frame_id);

  IOSPasswordManagerDriver* driver =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(web_state_.get(),
                                                               web_frame);
  if (!driver) {
    return;
  }

  password_manager::WebAuthnCredentialsDelegate* delegate =
      GetWebAuthnCredentialsDelegateForDriver(driver);
  if (!delegate) {
    return;
  }

  // TODO(crbug.com/460485496): Use an empty credential ID for now. The real
  // credential ID will come from the UI layer, i.e., from the accepted
  // FormSuggestion object.
  std::string credential_id;
  delegate->SelectPasskey(credential_id, base::DoNothing());
}

void IOSChromePasskeyClient::ShowCreationBottomSheet(RequestInfo request_info) {
  [command_handler_ showPasskeyCreationBottomSheet:request_info.request_id];
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
