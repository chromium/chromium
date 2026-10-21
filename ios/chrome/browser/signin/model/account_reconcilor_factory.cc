// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/model/account_reconcilor_factory.h"

#include <memory>
#include <utility>

#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"
#include "ios/chrome/browser/signin/model/signin_client_factory.h"

namespace ios {

AccountReconcilorFactory::AccountReconcilorFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key), "AccountReconcilor") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SigninClientFactory::GetInstance());
}

std::unique_ptr<KeyedService> AccountReconcilorFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto reconcilor = std::make_unique<AccountReconcilor>(
      identity_manager, SigninClientFactory::GetForProfile(profile),
      std::make_unique<signin::MirrorAccountReconcilorDelegate>(
          identity_manager));
  reconcilor->Initialize(true /* start_reconcile_if_tokens_available */);
  return reconcilor;
}

}  // namespace ios
