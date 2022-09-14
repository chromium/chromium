// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FAKE_H_
#define IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FAKE_H_

#include <memory>

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

namespace signin {
class IdentityManager;
}

namespace web {
class BrowserState;
}

// Fake implementation of AuthenticationService that can be used by tests.
class AuthenticationServiceFake : public AuthenticationService {
 public:
  static std::unique_ptr<KeyedService> CreateAuthenticationService(
      web::BrowserState* browser_state);

  ~AuthenticationServiceFake() override;

  void SignIn(ChromeIdentity* identity) override;

  void GrantSyncConsent(ChromeIdentity* identity) override;

  void SignOut(signin_metrics::ProfileSignout signout_source,
               bool force_clear_browsing_data,
               ProceduralBlock completion) override;

  ChromeIdentity* GetPrimaryIdentity(
      signin::ConsentLevel consent_level) const override;

  bool HasPrimaryIdentityManaged(
      signin::ConsentLevel consent_level) const override;

 private:
  AuthenticationServiceFake(
      PrefService* pref_service,
      SyncSetupService* sync_setup_service,
      ChromeAccountManagerService* account_manager_service,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service);

  // Internal method effectively signing out the user.
  void SignOutInternal(ProceduralBlock completion);

  __strong ChromeIdentity* primary_identity_;
  signin::ConsentLevel consent_level_ = signin::ConsentLevel::kSignin;

  // WeakPtrFactory should be last.
  base::WeakPtrFactory<AuthenticationServiceFake> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FAKE_H_
