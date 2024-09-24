// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_metrics_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

// static
supervised_user::SupervisedUserMetricsService*
SupervisedUserMetricsServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<supervised_user::SupervisedUserMetricsService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
SupervisedUserMetricsServiceFactory*
SupervisedUserMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserMetricsServiceFactory> instance;
  return instance.get();
}

SupervisedUserMetricsServiceFactory::SupervisedUserMetricsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SupervisedUserMetricsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SupervisedUserMetricsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  std::unique_ptr<supervised_user::SupervisedUserMetricsService ::
                      SupervisedUserMetricsServiceExtensionDelegate>
      extensions_metrics_delegate = nullptr;
  return std::make_unique<supervised_user::SupervisedUserMetricsService>(
      profile->GetPrefs(),
      SupervisedUserServiceFactory::GetForProfile(profile)->GetURLFilter(),
      std::move(extensions_metrics_delegate));
}
