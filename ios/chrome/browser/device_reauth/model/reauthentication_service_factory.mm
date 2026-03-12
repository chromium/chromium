// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"

// static
ReauthenticationServiceFactory* ReauthenticationServiceFactory::GetInstance() {
  static base::NoDestructor<ReauthenticationServiceFactory> instance;
  return instance.get();
}

// static
ReauthenticationService* ReauthenticationServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ReauthenticationService>(
      profile, /*create=*/true);
}

ReauthenticationServiceFactory::ReauthenticationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ReauthenticationService",
                                    ProfileSelection::kRedirectedInIncognito) {}

ReauthenticationServiceFactory::~ReauthenticationServiceFactory() = default;

std::unique_ptr<KeyedService>
ReauthenticationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  id<ReauthenticationProtocol> fake_reauth_module =
      tests_hook::GetFakeReauthenticationModule();
  if (fake_reauth_module) {
    return std::make_unique<ReauthenticationService>(fake_reauth_module);
  }

  return std::make_unique<ReauthenticationService>();
}
