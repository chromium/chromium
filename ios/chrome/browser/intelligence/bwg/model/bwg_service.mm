// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/public/web_state.h"
#import "ios/web/util/content_type_util.h"

BwgService::BwgService(ProfileIOS* profile,
                       AuthenticationService* auth_service,
                       signin::IdentityManager* identity_manager,
                       PrefService* pref_service) {
  profile_ = profile;
  auth_service_ = auth_service;
  identity_manager_ = identity_manager;
  identity_manager_->AddObserver(this);
  pref_service_ = pref_service;

  CheckGeminiEnterpriseEligibility();
}

BwgService::~BwgService() = default;

void BwgService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

#pragma mark - Public

bool BwgService::IsProfileEligibleForBwg() {
  AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  // If the account info was not found, the user is likely not authenticated.
  bool has_account_info = !account_info.IsEmpty();

  // Checks whether the account capabilities permit model execution.
  bool can_use_model_execution =
      has_account_info
          ? account_info.capabilities.can_use_model_execution_features() ==
                signin::Tribool::kTrue
          : false;

  // Checks the Chrome and Gemini Enterprise policies.
  bool is_disabled_by_policy =
      pref_service_->GetInteger(prefs::kGeminiEnabledByPolicy) == 1 ||
      is_disabled_by_gemini_policy_;

  bool is_eligible = can_use_model_execution && !is_disabled_by_policy &&
                     !profile_->IsOffTheRecord();

  base::UmaHistogramBoolean(kEligibilityHistogram, is_eligible);

  return is_eligible;
}

bool BwgService::IsBwgAvailableForWebState(web::WebState* web_state) {
  if (!IsProfileEligibleForBwg()) {
    return false;
  }
  // The web state is eligible for HTML and images that use http/https schemes.
  const GURL& url = web_state->GetVisibleURL();
  const std::string mime_type = web_state->GetContentsMimeType();
  const BOOL is_web_state_eligible =
      url.SchemeIsHTTPOrHTTPS() &&
      (web::IsContentTypeHtml(mime_type) || web::IsContentTypeImage(mime_type));

  return is_web_state_eligible;
}

#pragma mark - signin::IdentityManager::Observer

void BwgService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  CheckGeminiEnterpriseEligibility();
}

void BwgService::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
  }
}

#pragma mark - Private

void BwgService::CheckGeminiEnterpriseEligibility() {
  ios::provider::CheckGeminiEligibility(auth_service_, ^(BOOL eligible) {
    is_disabled_by_gemini_policy_ = !eligible;
  });
}
