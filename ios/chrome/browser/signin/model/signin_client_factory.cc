// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/model/signin_client_factory.h"

#include <utility>

#include "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/ios_chrome_signin_client.h"

SigninClientFactory::SigninClientFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key), "SigninClient") {
  DependsOn(ios::HostContentSettingsMapFactory::GetInstance());
}

std::unique_ptr<KeyedService> SigninClientFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<IOSChromeSigninClient>(
      profile, ios::HostContentSettingsMapFactory::GetForProfile(profile));
}
