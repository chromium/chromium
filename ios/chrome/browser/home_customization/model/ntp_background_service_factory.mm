// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/ntp_background_service_factory.h"

#import "base/no_destructor.h"
#import "components/themes/ntp_background_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
NtpBackgroundService* NtpBackgroundServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<NtpBackgroundService>(
      profile, /*create=*/true);
}

// static
NtpBackgroundServiceFactory* NtpBackgroundServiceFactory::GetInstance() {
  static base::NoDestructor<NtpBackgroundServiceFactory> instance;
  return instance.get();
}

NtpBackgroundServiceFactory::NtpBackgroundServiceFactory()
    : ProfileKeyedServiceFactoryIOS("NtpBackgroundService") {}

NtpBackgroundServiceFactory::~NtpBackgroundServiceFactory() {}

std::unique_ptr<KeyedService>
NtpBackgroundServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<NtpBackgroundService>(
      GetApplicationContext()->GetApplicationLocaleStorage(),
      profile->GetSharedURLLoaderFactory());
}
