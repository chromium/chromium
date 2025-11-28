// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"

#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/public/web_state.h"
#import "ios/web/util/content_type_util.h"

BwgService::BwgService(ProfileIOS* profile,
                       AuthenticationService* auth_service,
                       signin::IdentityManager* identity_manager,
                       PrefService* pref_service,
                       OptimizationGuideService* optimization_guide) {
  profile_ = profile;
  auth_service_ = auth_service;
  identity_manager_ = identity_manager;
  identity_manager_->AddObserver(this);
  pref_service_ = pref_service;

  // For managed accounts, we err on the side of caution and only show Gemini
  // entrypoints when we know whether they are eligible. Otherwise, we're OK
  // with having the entrypoint maybe disappear at a later time (actual Gemini
  // requests to ineligible accounts will fail regardless).
  is_disabled_by_gemini_policy_ =
      auth_service_ &&
      auth_service_->HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin);

  if (IsAskGeminiChipEnabled()) {
    optimization_guide_ = optimization_guide;
    optimization_guide_->RegisterOptimizationTypes(
        {optimization_guide::proto::GLIC_CONTEXTUAL_CUEING});
  }

  if (IsZeroStateSuggestionsEnabled()) {
    optimization_guide_ = optimization_guide;
    optimization_guide_->RegisterOptimizationTypes(
        {optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS});
  }

  CheckGeminiEnterpriseEligibility();
}

BwgService::~BwgService() = default;

void BwgService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

#pragma mark - Public

bool BwgService::IsProfileEligibleForBwg() {
  if (!IsGeminiAvailableForManagedAccounts()) {
    if (auth_service_ && auth_service_->HasPrimaryIdentityManaged(
                             signin::ConsentLevel::kSignin)) {
      return false;
    }
  }

  AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  bool tokens_ok =
      identity_manager_
          ->GetErrorStateOfRefreshTokenForAccount(account_info.account_id)
          .state() == GoogleServiceAuthError::NONE;

  // If the account info was not found, the user is likely not authenticated.
  bool has_account_info = !account_info.IsEmpty();

  // Checks whether the account capabilities permit model execution.
  bool can_use_model_execution =
      has_account_info
          ? account_info.capabilities.can_use_model_execution_features() ==
                signin::Tribool::kTrue
          : false;

  // Checks the Chrome and Gemini Enterprise policies.
  // kGeminiEnabledByPolicy is 0 for allowed, 1 for disallowed.
  bool is_disabled_by_policy =
      pref_service_->GetInteger(prefs::kGeminiEnabledByPolicy) == 1 ||
      is_disabled_by_gemini_policy_;

  bool is_eligible = can_use_model_execution && !is_disabled_by_policy &&
                     tokens_ok && !profile_->IsOffTheRecord();

  base::UmaHistogramBoolean(kEligibilityHistogram, is_eligible);

  return is_eligible;
}

bool BwgService::IsBwgAvailableForWebState(web::WebState* web_state) {
  if (!web_state || !IsProfileEligibleForBwg()) {
    return false;
  }

  return CanExtractPageContextForWebState(web_state);
}

#pragma mark - signin::IdentityManager::Observer

void BwgService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  CheckGeminiEnterpriseEligibility();
  if (ShouldDeleteGeminiConsentPref()) {
    // Clear the profile pref since it's syncable and should be account-scoped.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&BwgService::ClearConsentPref,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void BwgService::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
  }
}

void BwgService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  CheckGeminiEnterpriseEligibility();
}

#pragma mark - Private

void BwgService::CheckGeminiEnterpriseEligibility() {
  if (tests_hook::DisableGeminiEligibilityCheck()) {
    is_disabled_by_gemini_policy_ = false;
    return;
  }

  if (IsGeminiEligibilityAblationEnabled()) {
    return;
  }

  // No way to know if the user is blocked by Gemini Enterprise policy if the
  // auth service is null.
  if (!auth_service_) {
    is_disabled_by_gemini_policy_ = true;
    return;
  }

  eligibility_weak_ptr_factory_.InvalidateWeakPtrs();

  ios::provider::CheckGeminiEligibility(
      auth_service_, base::CallbackToBlock(base::BindOnce(
                         &BwgService::OnGeminiEligibilityResult,
                         eligibility_weak_ptr_factory_.GetWeakPtr())));
}

void BwgService::ClearConsentPref() {
  pref_service_->ClearPref(prefs::kIOSBwgConsent);
}

void BwgService::OnGeminiEligibilityResult(bool eligible) {
  is_disabled_by_gemini_policy_ = !eligible;
}
