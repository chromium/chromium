// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_metrics_service_factory.h"

#import <utility>

#import "components/supervised_user/core/browser/device_parental_controls.h"
#import "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_url_filtering_service_factory.h"

namespace supervised_user {

SupervisedUserMetricsServiceFactory::SupervisedUserMetricsServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "SupervisedUserMetricsService") {
  DependsOn(
      supervised_user::SupervisedUserUrlFilteringServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SupervisedUserMetricsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<SupervisedUserMetricsService>(
      profile->GetPrefs(),
      *supervised_user::SupervisedUserUrlFilteringServiceFactory::GetForProfile(
          profile),
      GetApplicationContext()->GetDeviceParentalControls(),
      /*extensions_metrics_delegate=*/nullptr);
}

}  // namespace supervised_user
