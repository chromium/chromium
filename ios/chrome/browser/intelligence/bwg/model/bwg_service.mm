// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

BwgService::BwgService(AuthenticationService* auth_service,
                       signin::IdentityManager* identity_manager,
                       PrefService* pref_service) {
  auth_service_ = auth_service;
  identity_manager_ = identity_manager;
  pref_service_ = pref_service;
}

BwgService::~BwgService() = default;

void BwgService::PresentOverlayOnViewController(
    UIViewController* base_view_controller,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context) {
  ios::provider::StartBwgOverlay(base_view_controller, auth_service_,
                                 std::move(page_context));
}

bool BwgService::IsEligibleForBWG() {
  AccountCapabilities capabilities =
      identity_manager_
          ->FindExtendedAccountInfo(identity_manager_->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin))
          .capabilities;

  bool can_use_model_execution =
      capabilities.can_use_model_execution_features() == signin::Tribool::kTrue;
  bool is_disabled_by_policy =
      pref_service_->GetInteger(prefs::kGeminiEnabledByPolicy) == 1;

  bool is_eligible = can_use_model_execution && !is_disabled_by_policy;

  base::UmaHistogramBoolean(kEligibilityHistogram, is_eligible);

  return is_eligible;
}
