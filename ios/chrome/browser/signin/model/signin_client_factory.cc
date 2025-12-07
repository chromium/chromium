// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/model/signin_client_factory.h"

#include "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/ios_chrome_signin_client.h"

// static
SigninClient* SigninClientFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<SigninClient>(profile,
                                                             /*create=*/true);
}

// static
SigninClientFactory* SigninClientFactory::GetInstance() {
  static base::NoDestructor<SigninClientFactory> instance;
  return instance.get();
}

SigninClientFactory::SigninClientFactory()
    : ProfileKeyedServiceFactoryIOS("SigninClient") {
  DependsOn(ios::HostContentSettingsMapFactory::GetInstance());
}

SigninClientFactory::~SigninClientFactory() = default;

std::unique_ptr<KeyedService> SigninClientFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<IOSChromeSigninClient>(
      profile, ios::HostContentSettingsMapFactory::GetForProfile(profile));
}
