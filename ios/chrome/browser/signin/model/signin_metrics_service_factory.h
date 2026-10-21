// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_METRICS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_METRICS_SERVICE_FACTORY_H_

#import <memory>

#import "components/signin/core/browser/signin_metrics_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that manages the `SigninMetricsService` service per profile.
class SigninMetricsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<SigninMetricsServiceFactory,
                                                SigninMetricsService> {
 public:
  explicit SigninMetricsServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_METRICS_SERVICE_FACTORY_H_
