// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/data_sharing/model/features.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_configuration.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/public/provider/chrome/browser/share_kit/share_kit_api.h"

// static
ShareKitService* ShareKitServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<ShareKitService*>(
      GetInstance()->GetServiceForProfileAs<ShareKitService>(profile,
                                                             /*create=*/true));
}

// static
ShareKitServiceFactory* ShareKitServiceFactory::GetInstance() {
  static base::NoDestructor<ShareKitServiceFactory> instance;
  return instance.get();
}

ShareKitServiceFactory::ShareKitServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ShareKitService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(AuthenticationServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
}

ShareKitServiceFactory::~ShareKitServiceFactory() = default;

std::unique_ptr<KeyedService> ShareKitServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = static_cast<ProfileIOS*>(context);

  if (!IsSharedTabGroupsJoinEnabled(profile) &&
      !IsSharedTabGroupsCreateEnabled(profile)) {
    return nullptr;
  }

  ShareKitServiceConfiguration configuration;
  configuration.identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  configuration.authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  configuration.data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  return ios::provider::CreateShareKitService(configuration);
}
