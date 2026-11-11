// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/ios_chrome_remote_actor_credential_sharing_service_factory.h"

#import <memory>
#import <utility>

#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_service_impl.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

IOSChromeRemoteActorCredentialSharingServiceFactory::
    IOSChromeRemoteActorCredentialSharingServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "RemoteActorCredentialSharingService") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSChromeRemoteActorCredentialSharingServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!password_manager::features::RemoteActorCredentialSharingEnabled()) {
    return nullptr;
  }
  return std::make_unique<
      password_manager::RemoteActorCredentialSharingServiceImpl>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory());
}
