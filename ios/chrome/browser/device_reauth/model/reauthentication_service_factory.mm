// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"

#import <utility>

#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

ReauthenticationServiceFactory::ReauthenticationServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "ReauthenticationService",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService>
ReauthenticationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ReauthenticationService>(
      tests_hook::GetFakeReauthenticationModule()
          ?: [[ReauthenticationModule alloc] init]);
}
