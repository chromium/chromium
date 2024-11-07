// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_BROWSER_AGENT_H_

#import <CoreFoundation/CoreFoundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

// Service that listens for PasskeyModel changes and triggers an infobar
// notification when required.
class CredentialProviderBrowserAgent
    : public BrowserUserData<CredentialProviderBrowserAgent>,
      public BrowserObserver,
      public webauthn::PasskeyModel::Observer {
 public:
  ~CredentialProviderBrowserAgent() override;

  // Sets whether the infobar is allowed to be shown at this time. Only changes
  // coming from the credential provider migrator should be allowed to show the
  // infobar.
  void SetInfobarAllowed(bool allowed);

 private:
  friend class BrowserUserData<CredentialProviderBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit CredentialProviderBrowserAgent(Browser* browser);

  void DisplayInfoBar(const sync_pb::WebauthnCredentialSpecifics& passkey);

  void RemoveObservers();

  // BrowserObserver::
  void BrowserDestroyed(Browser* browser) override;

  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

  // The owning Browser
  raw_ptr<Browser> browser_;

  // Owned by the IOSPasskeyModelFactory which should outlive this class
  raw_ptr<webauthn::PasskeyModel> model_;

  base::ScopedObservation<Browser, BrowserObserver> browser_observation_{this};

  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      model_observation_{this};

  bool infobar_allowed_ = false;
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_BROWSER_AGENT_H_
