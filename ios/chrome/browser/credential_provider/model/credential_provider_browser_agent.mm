// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_browser_agent.h"

#import "base/time/time.h"
#import "components/infobars/core/infobar.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/credential_provider/model/ios_credential_provider_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/web/public/web_state.h"

// Maximum amount of time since a passkey was created for it to count as a
// recently added passkey.
static constexpr base::TimeDelta kRecentlyAddedDelay = base::Seconds(5);

BROWSER_USER_DATA_KEY_IMPL(CredentialProviderBrowserAgent)

CredentialProviderBrowserAgent::CredentialProviderBrowserAgent(Browser* browser)
    : browser_(browser),
      model_(IOSPasskeyModelFactory::GetForProfile(
          // Here, we want to observe the user's passkey model, so we need the
          // original profile.
          browser_->GetProfile()->GetOriginalProfile())) {
  if (model_) {
    model_observation_.Observe(model_.get());
    browser_observation_.Observe(browser_.get());
  }
}

CredentialProviderBrowserAgent::~CredentialProviderBrowserAgent() = default;

void CredentialProviderBrowserAgent::DisplayInfoBar() {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  if (!web_state || !web_state->IsVisible()) {
    return;
  }

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  if (!infobar_manager) {
    return;
  }

  // Here, we are observing the user's passkey model, so we need the original
  // profile to get the user's identity.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          browser_->GetProfile()->GetOriginalProfile());

  if (!identity_manager) {
    return;
  }

  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  infobar_manager->AddInfoBar(CreateConfirmInfoBar(
      IOSCredentialProviderInfoBarDelegate::Create(std::move(account.email))));
}

void CredentialProviderBrowserAgent::RemoveObservers() {
  model_observation_.Reset();
  browser_observation_.Reset();
}

void CredentialProviderBrowserAgent::BrowserDestroyed(Browser* browser) {
  RemoveObservers();
}

void CredentialProviderBrowserAgent::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  // If a passkey was added recently, display the infobar.
  for (const webauthn::PasskeyModelChange& change : changes) {
    if (change.type() == webauthn::PasskeyModelChange::ChangeType::ADD) {
      const sync_pb::WebauthnCredentialSpecifics& passkey = change.passkey();
      base::TimeDelta delay =
          base::Time::Now() -
          base::Time::FromMillisecondsSinceUnixEpoch(passkey.creation_time());
      if (delay.is_positive() && (delay < kRecentlyAddedDelay)) {
        DisplayInfoBar();
        break;
      }
    }
  }
}

void CredentialProviderBrowserAgent::OnPasskeyModelShuttingDown() {
  RemoveObservers();
}

void CredentialProviderBrowserAgent::OnPasskeyModelIsReady(bool is_ready) {}
