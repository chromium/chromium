// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/gemini/model/glic_service.h"

#import <memory>

#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/public/provider/chrome/browser/glic/glic_api.h"

GlicService::GlicService(AuthenticationService* auth_service,
                         signin::IdentityManager* identity_manager) {
  auth_service_ = auth_service;
  identity_manager_ = identity_manager;
}

GlicService::~GlicService() = default;

void GlicService::PresentOverlayOnViewController(
    UIViewController* base_view_controller,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context) {
  ios::provider::StartGlicOverlay(base_view_controller, auth_service_,
                                  std::move(page_context));
}

bool GlicService::IsEligibleForGlic() {
  // TODO(crbug.com/419066154): Check other conditions, such as enterprise.

  AccountCapabilities capabilities =
      identity_manager_
          ->FindExtendedAccountInfo(identity_manager_->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin))
          .capabilities;

  return capabilities.can_use_model_execution_features() ==
         signin::Tribool::kTrue;
}
