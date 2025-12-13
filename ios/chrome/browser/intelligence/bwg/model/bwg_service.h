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
class CoreAccountInfo;
class IdentityManager;
}  // namespace signin
class OptimizationGuideService;
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
             PrefService* pref_service,
             OptimizationGuideService* optimization_guide);
  ~BwgService() override;
  void Shutdown() override;

  // Returns whether the current profile is eligible for BWG.
  bool IsProfileEligibleForBwg();

  // Whether BWG is available for a given web state.
  bool IsBwgAvailableForWebState(web::WebState* web_state);

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
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
  raw_ptr<PrefService, DanglingUntriaged> pref_service_ = nullptr;

  // The optimization guide service for model execution and page metadata.
  raw_ptr<OptimizationGuideService> optimization_guide_ = nullptr;

  // Whether the user is ineligible by the Gemini Enterprise policy (not Chrome
  // Enterprise).
  bool is_disabled_by_gemini_policy_ = false;

  // Checks if the account is eligible for Gemini Enterprise and populates
  // `is_disabled_by_gemini_policy_`.
  void CheckGeminiEnterpriseEligibility();

  // Clears the Gemini consent profile pref.
  void ClearConsentPref();

  // Invoked when the eligibility check is done.
  void OnGeminiEligibilityResult(bool eligible);

  // Weak pointer factory for Gemini eligibility checks.
  base::WeakPtrFactory<BwgService> eligibility_weak_ptr_factory_{this};

  // Generic weak pointer factory.
  base::WeakPtrFactory<BwgService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_H_
