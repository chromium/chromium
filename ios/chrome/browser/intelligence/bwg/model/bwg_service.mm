// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_configuration.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

namespace {
// Helper to convert PageContextWrapperError to BWGPageContextState.
ios::provider::BWGPageContextState BWGPageContextFromPageContextWrapperError(
    PageContextWrapperError error) {
  switch (error) {
    case PageContextWrapperError::kForceDetachError:
      return ios::provider::BWGPageContextState::kProtected;
    default:
      return ios::provider::BWGPageContextState::kError;
  }
}
}  // namespace

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
    base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                   PageContextWrapperError> expected_page_context) {
  BWGConfiguration* config = [[BWGConfiguration alloc] init];
  config.baseViewController = base_view_controller;
  config.authService = auth_service_;

  std::unique_ptr<optimization_guide::proto::PageContext> pageContext = nullptr;
  if (expected_page_context.has_value()) {
    pageContext = std::move(expected_page_context.value());
    config.BWGPageContextState =
        ios::provider::BWGPageContextState::kSuccessfullyAttached;
  } else {
    config.BWGPageContextState = BWGPageContextFromPageContextWrapperError(
        expected_page_context.error());
  }

  config.pageContext = std::move(pageContext);
  ios::provider::StartBwgOverlay(config);
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
