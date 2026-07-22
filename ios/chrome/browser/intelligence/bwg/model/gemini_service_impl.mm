// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_impl.h"

#import <optional>

#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/content_type_util.h"
#import "ios/web/public/web_state.h"

GeminiServiceImpl::GeminiServiceImpl(
    ProfileIOS* profile,
    AuthenticationService* auth_service,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    OptimizationGuideService* optimization_guide) {
  profile_ = profile;
  auth_service_ = auth_service;
  identity_manager_ = identity_manager;
  identity_manager_observation_.Observe(identity_manager_);
  pref_service_ = pref_service;
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kGeminiEnabledByPolicy,
      base::BindRepeating(&GeminiServiceImpl::OnPolicyPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kGenAiEnabledByPolicy,
      base::BindRepeating(&GeminiServiceImpl::OnPolicyPrefChanged,
                          base::Unretained(this)));

  optimization_guide_ = optimization_guide;
  optimization_guide_->RegisterOptimizationTypes(
      {optimization_guide::proto::GLIC_CONTEXTUAL_CUEING});

  if (IsZeroStateSuggestionsEnabled()) {
    optimization_guide_ = optimization_guide;
    optimization_guide_->RegisterOptimizationTypes(
        {optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS});
  }

  if (!IsPageActionMenuAuthFlowEnabled() || IsChromeNextIaEnabled()) {
    CheckGeminiEnterpriseEligibilityIfNeeded();
  }
}

GeminiServiceImpl::~GeminiServiceImpl() = default;

void GeminiServiceImpl::Shutdown() {
  identity_manager_observation_.Reset();
}

#pragma mark - Public

void GeminiServiceImpl::AddObserver(GeminiService::Observer* observer) {
  observers_.AddObserver(observer);
}

void GeminiServiceImpl::RemoveObserver(GeminiService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool GeminiServiceImpl::IsProfileEligibleForGemini() {
  return !GeminiIneligibilityForProfile().has_value();
}

bool GeminiServiceImpl::IsWorkspacePolicyCheckPending() {
  // If the user is signed out, no check is performed or pending.
  if (!auth_service_ || !auth_service_->HasPrimaryIdentity()) {
    return false;
  }
  return !is_disabled_by_gemini_policy_.has_value();
}

std::optional<gemini::IneligibilityReasons>
GeminiServiceImpl::GeminiIneligibilityForProfile() {
  AccountInfo account_info = PrimaryAccountInfo();
  const bool has_capabilities =
      gemini::HasGeminiInChromeCapability(account_info);
  const bool allowed_by_enterprise =
      gemini::GeminiAllowedByPolicy(pref_service_);
  const bool authenticated =
      !account_info.IsEmpty() &&
      identity_manager_
              ->GetErrorStateOfRefreshTokenForAccount(account_info.account_id)
              .state() == GoogleServiceAuthError::NONE;

  // For managed accounts, we err on the side of caution and only show Gemini
  // entrypoints when we know whether they are eligible. Otherwise, we're OK
  // with having the entrypoint maybe disappear at a later time (actual Gemini
  // requests to ineligible accounts will fail regardless).
  const bool is_managed_account =
      auth_service_ && auth_service_->HasPrimaryIdentityManaged();
  const bool is_eligible =
      has_capabilities && allowed_by_enterprise &&
      !is_disabled_by_gemini_policy_.value_or(is_managed_account) &&
      authenticated && !profile_->IsOffTheRecord();
  // We ignore the gemini workspace log until we actually get the response to
  // `is_disabled_by_gemini_policy_`.
  const gemini::IneligibilityReasons ineligibility_reasons =
      gemini::IneligibilityReasons()
          .set_workspace(is_disabled_by_gemini_policy_.value_or(false))
          .set_chrome_enterprise(!allowed_by_enterprise)
          .set_account_capability(!has_capabilities)
          .set_authentication(!authenticated);
  RecordGeminiIneligibilityReasons(ineligibility_reasons);
  RecordGeminiEligibility(is_eligible);
  if (is_eligible) {
    LogFirstRunState();
    return std::nullopt;
  }

  return ineligibility_reasons;
}

#pragma mark - signin::IdentityManager::Observer

void GeminiServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  // Only check eligibility if the user is signed in. If they signed out,
  // clear any cached workspace policy status.
  if (auth_service_ && auth_service_->HasPrimaryIdentity()) {
    CheckGeminiEnterpriseEligibility();
  } else {
    // Invalidate any pending callbacks since the user has signed out.
    eligibility_weak_ptr_factory_.InvalidateWeakPtrs();
    SetIsDisabledByGeminiPolicy(std::nullopt);
  }
  if (ShouldDeleteGeminiConsentPref()) {
    // Clear the profile pref since it's syncable and should be account-scoped.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&GeminiServiceImpl::ClearConsentPref,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void GeminiServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observation_.Reset();
}

void GeminiServiceImpl::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // Only check eligibility for the primary account if the user is signed in.
  if (auth_service_ && auth_service_->HasPrimaryIdentity() &&
      account_info.account_id == identity_manager_->GetPrimaryAccountId(
                                     signin::ConsentLevel::kSignin)) {
    CheckGeminiEnterpriseEligibility();
  }
}

void GeminiServiceImpl::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (account_info.account_id ==
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    // Account capabilities (like age/model execution) may have finished
    // loading from the server, changing overall eligibility.
    for (auto& observer : observers_) {
      observer.OnGeminiEligibilityChanged();
    }
  }
}

void GeminiServiceImpl::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (account_info.account_id ==
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    // The user's credentials or login session may have expired or become
    // invalid, changing authentication eligibility.
    for (auto& observer : observers_) {
      observer.OnGeminiEligibilityChanged();
    }
  }
}

#pragma mark - Private

void GeminiServiceImpl::OnPolicyPrefChanged() {
  // Chrome Enterprise policies (like Gemini allowed/disallowed by policy)
  // may have been updated dynamically.
  for (auto& observer : observers_) {
    observer.OnGeminiEligibilityChanged();
  }
}

void GeminiServiceImpl::CheckGeminiEnterpriseEligibility() {
  if (tests_hook::DisableGeminiEligibilityCheck()) {
    SetIsDisabledByGeminiPolicy(false);
    return;
  }

  if (IsGeminiEligibilityAblationEnabled()) {
    return;
  }

  // CheckGeminiEnterpriseEligibility should only be called when the user is
  // signed in.
  CHECK(auth_service_ && auth_service_->HasPrimaryIdentity());

  eligibility_weak_ptr_factory_.InvalidateWeakPtrs();

  SetIsDisabledByGeminiPolicy(std::nullopt);

  ios::provider::CheckGeminiEligibility(
      auth_service_, base::CallbackToBlock(base::BindOnce(
                         &GeminiServiceImpl::OnGeminiEligibilityResult,
                         eligibility_weak_ptr_factory_.GetWeakPtr())));
}

void GeminiServiceImpl::CheckGeminiEnterpriseEligibilityIfNeeded() {
  // If the user is signed out, no check is needed.
  if (!auth_service_ || !auth_service_->HasPrimaryIdentity()) {
    return;
  }
  if (!is_disabled_by_gemini_policy_.has_value() &&
      !eligibility_weak_ptr_factory_.HasWeakPtrs()) {
    CheckGeminiEnterpriseEligibility();
  }
}

bool GeminiServiceImpl::HasGeminiInChromeCapability() {
  return gemini::HasGeminiInChromeCapability(PrimaryAccountInfo());
}

bool GeminiServiceImpl::HasModelExecutionCapability() {
  return gemini::HasModelExecutionCapability(PrimaryAccountInfo());
}

void GeminiServiceImpl::ClearConsentPref() {
  pref_service_->ClearPref(prefs::kIOSBwgConsent);
  pref_service_->ClearPref(prefs::kIOSGeminiLiveConsent);
  pref_service_->ClearPref(prefs::kIOSGeminiLiveIntroPlayed);
}

void GeminiServiceImpl::LogFirstRunState() {
  gemini::FirstRunState state = gemini::CurrentFirstRunState(pref_service_);
  if (!last_logged_first_run_state_.has_value() ||
      last_logged_first_run_state_.value() != state) {
    RecordGeminiFirstRunState(state);
    last_logged_first_run_state_ = state;
  }
}

void GeminiServiceImpl::OnGeminiEligibilityResult(bool eligible) {
  SetIsDisabledByGeminiPolicy(!eligible);
}

void GeminiServiceImpl::SetIsDisabledByGeminiPolicy(
    std::optional<bool> disabled) {
  if (is_disabled_by_gemini_policy_ == disabled) {
    return;
  }
  is_disabled_by_gemini_policy_ = disabled;
  for (auto& observer : observers_) {
    observer.OnGeminiEligibilityChanged();
  }
}

AccountInfo GeminiServiceImpl::PrimaryAccountInfo() const {
  return identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
}
