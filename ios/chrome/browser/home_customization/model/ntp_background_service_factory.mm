// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/ntp_background_service_factory.h"

#import <utility>

#import "components/themes/ntp_background_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

NtpBackgroundServiceFactory::NtpBackgroundServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "NtpBackgroundService") {}

std::unique_ptr<KeyedService>
NtpBackgroundServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<NtpBackgroundService>(
      GetApplicationContext()->GetApplicationLocaleStorage(),
      profile->GetSharedURLLoaderFactory());
}
