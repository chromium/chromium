// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_url_filtering_service_factory.h"

#import <memory>

#import "base/check_deref.h"
#import "components/supervised_user/core/browser/device_parental_controls_url_filter.h"
#import "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_platform_delegate.h"
#import "ios/chrome/common/channel_info.h"

namespace supervised_user {

// static
SupervisedUserUrlFilteringService*
SupervisedUserUrlFilteringServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<SupervisedUserUrlFilteringService>(
          profile, /*create=*/true);
}

// static
SupervisedUserUrlFilteringServiceFactory*
SupervisedUserUrlFilteringServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserUrlFilteringServiceFactory> instance;
  return instance.get();
}

SupervisedUserUrlFilteringServiceFactory::
    SupervisedUserUrlFilteringServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SupervisedUserUrlFilteringService") {
  // Temporary dependency on the SupervisedUserService instance to allow
  // migration of legacy SupervisedUserURLFilter methods that are called through
  // the SupervisedUserService: service_->GetURLFilter()->Method(). Remove once
  // all callers are migrated to the SupervisedUserURLFilteringService.
  // TODO(crbug.com/469336110): Remove this dependency after migration.
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SupervisedUserUrlFilteringServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  SupervisedUserServicePlatformDelegate platform_delegate(profile);
  return std::make_unique<SupervisedUserUrlFilteringService>(
      CHECK_DEREF(SupervisedUserServiceFactory::GetForProfile(profile)),
      std::make_unique<DeviceParentalControlsUrlFilter>(
          GetApplicationContext()->GetDeviceParentalControls(),
          std::make_unique<KidsChromeManagementURLCheckerClient>(
              GetApplicationContext()->GetSharedURLLoaderFactory(),
              platform_delegate.GetCountryCode(),
              platform_delegate.GetChannel())));
}
}  // namespace supervised_user
