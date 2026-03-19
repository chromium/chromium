// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_wallet_pass_access_manager_factory.h"

#import "components/autofill/core/browser/network/autofill_ai/fake_wallet_pass_access_manager.h"
#import "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#import "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager_impl.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/wallet/core/browser/network/wallet_http_client_impl.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
autofill::WalletPassAccessManager*
IOSWalletPassAccessManagerFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<autofill::WalletPassAccessManager>(
          profile, /*create=*/true);
}

// static
IOSWalletPassAccessManagerFactory*
IOSWalletPassAccessManagerFactory::GetInstance() {
  static base::NoDestructor<IOSWalletPassAccessManagerFactory> instance;
  return instance.get();
}

IOSWalletPassAccessManagerFactory::IOSWalletPassAccessManagerFactory()
    : ProfileKeyedServiceFactoryIOS("WalletPassAccessManager") {
  DependsOn(IOSAutofillEntityDataManagerFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSWalletPassAccessManagerFactory::~IOSWalletPassAccessManagerFactory() =
    default;

std::unique_ptr<KeyedService>
IOSWalletPassAccessManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiWalletPrivatePasses)) {
    return nullptr;
  }

  autofill::EntityDataManager* data_manager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile);

  if (base::FeatureList::IsEnabled(
          autofill::features::debug::kFakeWalletApiResponses)) {
    return std::make_unique<autofill::FakeWalletPassAccessManager>(
        data_manager);
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  return std::make_unique<autofill::WalletPassAccessManagerImpl>(
      std::make_unique<wallet::WalletHttpClientImpl>(
          identity_manager, profile->GetSharedURLLoaderFactory()),
      data_manager);
}
