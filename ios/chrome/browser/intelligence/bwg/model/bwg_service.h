// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_H_

#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"

class AuthenticationService;
namespace signin {
class IdentityManager;
}  // namespace signin
class PrefService;
class ProfileIOS;
namespace web {
class WebState;
}

// A browser-context keyed service for BWG.
class BwgService : public KeyedService,
                   public signin::IdentityManager::Observer {
 public:
  BwgService(ProfileIOS* profile,
             AuthenticationService* auth_service,
             signin::IdentityManager* identity_manager,
             PrefService* pref_service);
  ~BwgService() override;
  void Shutdown() override;

  // Returns whether the current profile is eligible for BWG.
  bool IsProfileEligibleForBwg();

  // Whether BWG is available for a given web state.
  bool IsBwgAvailableForWebState(web::WebState* web_state);

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  // The associated profile.
  raw_ptr<ProfileIOS> profile_;

  // AuthenticationService used to check the user's account status.
  raw_ptr<AuthenticationService> auth_service_ = nullptr;

  // Identity manager used to check account capabilities.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // The PrefService associated with the Profile.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Whether the user is ineligible by the Gemini Enterprise policy (not Chrome
  // Enterprise).
  bool is_disabled_by_gemini_policy_ = false;

  // Checks if the account is eligible for Gemini Enterprise and populates
  // `is_disabled_by_gemini_policy_`.
  void CheckGeminiEnterpriseEligibility();
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_H_
