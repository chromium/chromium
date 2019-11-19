// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FAKE_H_
#define IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FAKE_H_

#include <memory>

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

  void SignOut(signin_metrics::ProfileSignout signout_source,
               ProceduralBlock completion) override;

  void SetHaveAccountsChangedWhileInBackground(bool changed);

  bool HaveAccountsChangedWhileInBackground() const override;

  bool IsAuthenticated() const override;

  ChromeIdentity* GetAuthenticatedIdentity() const override;

 private:
  AuthenticationServiceFake(PrefService* pref_service,
                            SyncSetupService* sync_setup_service,
                            signin::IdentityManager* identity_manager,
                            syncer::SyncService* sync_service);

  __strong ChromeIdentity* authenticated_identity_;
  bool have_accounts_changed_while_in_background_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FAKE_H_
