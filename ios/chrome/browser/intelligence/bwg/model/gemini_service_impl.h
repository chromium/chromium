// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_IMPL_H_

#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"

class AuthenticationService;
struct CoreAccountInfo;
namespace gemini {
struct IneligibilityReasons;
enum class FREState;
}  // namespace gemini
namespace signin {
class IdentityManager;
}  // namespace signin
class OptimizationGuideService;
class PrefService;
class ProfileIOS;

// Implementation of GeminiService.
class GeminiServiceImpl : public GeminiService,
                          public signin::IdentityManager::Observer {
 public:
  GeminiServiceImpl(ProfileIOS* profile,
                    AuthenticationService* auth_service,
                    signin::IdentityManager* identity_manager,
                    PrefService* pref_service,
                    OptimizationGuideService* optimization_guide);
  ~GeminiServiceImpl() override;
  void Shutdown() override;

  // GeminiService:
  bool IsProfileEligibleForGemini() override;
  std::optional<gemini::IneligibilityReasons> GeminiIneligibilityForProfile()
      override;
  bool IsWorkspacePolicyCheckPending() override;
  void CheckGeminiEnterpriseEligibilityIfNeeded() override;
  bool HasGeminiInChromeCapability() override;
  bool HasModelExecutionCapability() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  friend class GeminiServiceImplTest;

  // The associated profile.
  raw_ptr<ProfileIOS> profile_;

  // AuthenticationService used to check the user's account status.
  raw_ptr<AuthenticationService> auth_service_;

  // Identity manager used to check account capabilities.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // The PrefService associated with the Profile.
  raw_ptr<PrefService> pref_service_;

  // The optimization guide service for model execution and page metadata.
  raw_ptr<OptimizationGuideService> optimization_guide_;

  // Whether the user is ineligible by the Gemini Enterprise policy (not Chrome
  // Enterprise).
  std::optional<bool> is_disabled_by_gemini_policy_;

  // The last FRE state for Gemini to have been logged this session.
  std::optional<gemini::FREState> last_logged_fre_state_;

  // Checks if the account is eligible for Gemini Enterprise and populates
  // `is_disabled_by_gemini_policy_`.
  void CheckGeminiEnterpriseEligibility();

  // Clears the Gemini consent profile pref.
  void ClearConsentPref();

  // Logs the current FRE state whenever deemed necessary.
  void LogFREState();

  // Invoked when the eligibility check is done.
  void OnGeminiEligibilityResult(bool eligible);

  // Returns the extended AccountInfo for the primary account.
  AccountInfo PrimaryAccountInfo() const;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Weak pointer factory for Gemini eligibility checks.
  base::WeakPtrFactory<GeminiServiceImpl> eligibility_weak_ptr_factory_{this};

  // Generic weak pointer factory.
  base::WeakPtrFactory<GeminiServiceImpl> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_IMPL_H_
