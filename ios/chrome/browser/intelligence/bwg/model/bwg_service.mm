// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

BwgService::BwgService(signin::IdentityManager* identity_manager,
                       PrefService* pref_service) {
  identity_manager_ = identity_manager;
  pref_service_ = pref_service;
}

BwgService::~BwgService() = default;

bool BwgService::IsEligibleForBwg() {
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

  // Checks the enterprise policy.
  bool is_disabled_by_policy =
      pref_service_->GetInteger(prefs::kGeminiEnabledByPolicy) == 1;

  bool is_eligible = can_use_model_execution && !is_disabled_by_policy;

  base::UmaHistogramBoolean(kEligibilityHistogram, is_eligible);

  return is_eligible;
}
