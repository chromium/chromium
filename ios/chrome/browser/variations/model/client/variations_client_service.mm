// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/client/variations_client_service.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

VariationsClientService::VariationsClientService(ProfileIOS* profile)
    : profile_(profile) {}

VariationsClientService::~VariationsClientService() = default;

bool VariationsClientService::IsOffTheRecord() const {
  return profile_->IsOffTheRecord();
}

variations::mojom::VariationsHeadersPtr
VariationsClientService::GetVariationsHeaders() const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  bool is_signed_in = identity_manager && identity_manager->HasPrimaryAccount(
                                              signin::ConsentLevel::kSignin);
  return variations::VariationsIdsProvider::GetInstance()->GetClientDataHeaders(
      is_signed_in);
}
