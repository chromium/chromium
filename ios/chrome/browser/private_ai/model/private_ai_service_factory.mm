// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/private_ai/model/private_ai_service_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/private_ai/features.h"
#import "components/private_ai/ios/private_ai_network_driver_ios.h"
#import "components/private_ai/ios/private_ai_oak_session_driver_ios.h"
#import "components/private_ai/phosphor/blind_sign_auth_factory_impl.h"
#import "components/private_ai/private_ai_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/common/channel_info.h"

// static
private_ai::PrivateAiService* PrivateAiServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<private_ai::PrivateAiService>(
      profile, /*create=*/true);
}

// static
PrivateAiServiceFactory* PrivateAiServiceFactory::GetInstance() {
  static base::NoDestructor<PrivateAiServiceFactory> instance;
  return instance.get();
}

PrivateAiServiceFactory::PrivateAiServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PrivateAiService") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

PrivateAiServiceFactory::~PrivateAiServiceFactory() = default;

std::unique_ptr<KeyedService> PrivateAiServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!private_ai::PrivateAiService::CanPrivateAiBeEnabled(::GetChannel())) {
    return nullptr;
  }

  auto network_driver =
      std::make_unique<private_ai::PrivateAiNetworkDriverIOS>();
  auto oak_session_driver =
      std::make_unique<private_ai::PrivateAiOakSessionDriverIOS>();

  // The PrivateAiService calls a method on the BlindSignAuthFactory in its
  // constructor and does not reference it afterward. Thus, the factory can
  // be safely stack-allocated here.
  // TODO(crbug.com/527423165): Refactor PrivateAiService to take the
  // BlindSignAuthFactory as an std::unique_ptr.
  private_ai::phosphor::BlindSignAuthFactoryImpl bsa_factory;

  return std::make_unique<private_ai::PrivateAiService>(
      IdentityManagerFactory::GetForProfile(profile), &bsa_factory,
      profile->GetSharedURLLoaderFactory(), std::move(network_driver),
      std::move(oak_session_driver),
      /*network_context=*/nullptr, private_ai::kPrivateAiUrl.Get(),
      private_ai::PrivateAiService::GetApiKey(::GetChannel()),
      private_ai::kPrivateAiProxyServerUrl.Get(),
      base::FeatureList::IsEnabled(private_ai::kPrivateAiUseTokenAttestation),
      ::GetChannel());
}
