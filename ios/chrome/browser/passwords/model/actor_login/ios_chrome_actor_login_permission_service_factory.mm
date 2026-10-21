// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/actor_login/ios_chrome_actor_login_permission_service_factory.h"

#import <memory>
#import <utility>

#import "components/password_manager/core/browser/actor_login/internal/actor_login_permission_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

using actor_login::ActorLoginPermissionService;
using actor_login::ActorLoginPermissionServiceImpl;

IOSChromeActorLoginPermissionServiceFactory::
    IOSChromeActorLoginPermissionServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "ActorLoginPermissionService") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSChromeActorLoginPermissionServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ActorLoginPermissionServiceImpl>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory());
}
