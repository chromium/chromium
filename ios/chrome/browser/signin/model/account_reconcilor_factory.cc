// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/model/account_reconcilor_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"
#include "ios/chrome/browser/signin/model/signin_client_factory.h"

namespace ios {

AccountReconcilorFactory::AccountReconcilorFactory()
    : ProfileKeyedServiceFactoryIOS("AccountReconcilor") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SigninClientFactory::GetInstance());
}

AccountReconcilorFactory::~AccountReconcilorFactory() {}

// static
AccountReconcilor* AccountReconcilorFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AccountReconcilor>(
      profile, /*create=*/true);
}

// static
AccountReconcilorFactory* AccountReconcilorFactory::GetInstance() {
  static base::NoDestructor<AccountReconcilorFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService> AccountReconcilorFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto reconcilor = std::make_unique<AccountReconcilor>(
      identity_manager, SigninClientFactory::GetForProfile(profile),
      std::make_unique<signin::MirrorAccountReconcilorDelegate>(
          identity_manager));
  reconcilor->Initialize(true /* start_reconcile_if_tokens_available */);
  return reconcilor;
}

}  // namespace ios
