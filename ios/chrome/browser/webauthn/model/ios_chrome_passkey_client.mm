// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_chrome_passkey_client.h"

#import "base/functional/callback.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"
#import "components/webauthn/ios/features.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import "ios/chrome/browser/credential_provider/model/credential_provider_browser_agent.h"
#endif

IOSChromePasskeyClient::IOSChromePasskeyClient(web::WebState* web_state) {
  CHECK(web_state);
  web_state_ = web_state->GetWeakPtr();
}

IOSChromePasskeyClient::~IOSChromePasskeyClient() {}

bool IOSChromePasskeyClient::PerformUserVerification() {
  // TODO(crbug.com/385174410): Perform user verification.
  // See PasskeyKeychainProvider::Reauthenticate and ReauthenticationModule.
  return false;
}

void IOSChromePasskeyClient::FetchKeys(webauthn::ReauthenticatePurpose purpose,
                                       webauthn::KeysFetchedCallback callback) {
  // TODO(crbug.com/385174410): Fetch the keys. See PasskeyKeychainProvider.
  std::move(callback).Run({});
}

void IOSChromePasskeyClient::ShowSuggestionBottomSheet() {
  // TODO(crbug.com/385174410): Open the suggestion bottom sheet.
  // See CredentialSuggestionBottomSheet* classes.
}

void IOSChromePasskeyClient::AllowPasskeyCreationInfobar(bool allowed) {
#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  if (!web_state_) {
    return;
  }

  ProfileIOS* const profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());

  BrowserList* browserList = BrowserListFactory::GetForProfile(profile);
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
