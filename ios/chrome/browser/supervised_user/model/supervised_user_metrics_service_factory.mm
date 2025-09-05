// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_metrics_service_factory.h"

#import "base/no_destructor.h"
#import "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

// static
supervised_user::SupervisedUserMetricsService*
SupervisedUserMetricsServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<supervised_user::SupervisedUserMetricsService>(
          profile, /*create=*/true);
}

// static
SupervisedUserMetricsServiceFactory*
SupervisedUserMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserMetricsServiceFactory> instance;
  return instance.get();
}

SupervisedUserMetricsServiceFactory::SupervisedUserMetricsServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SupervisedUserMetricsService") {
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SupervisedUserMetricsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<supervised_user::SupervisedUserMetricsService>(
      profile->GetPrefs(),
      *SupervisedUserServiceFactory::GetForProfile(profile),
      /*extensions_metrics_delegate=*/nullptr,
      /*metrics_service_accessor_delegate=*/nullptr);
}
