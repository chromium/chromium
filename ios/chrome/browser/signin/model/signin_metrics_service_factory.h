// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_METRICS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_METRICS_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class SigninMetricsService;

// Singleton that manages the `SigninMetricsService` service per profile.
class SigninMetricsServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static SigninMetricsService* GetForProfile(ProfileIOS* profile);
  static SigninMetricsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SigninMetricsServiceFactory>;

  SigninMetricsServiceFactory();
  ~SigninMetricsServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_METRICS_SERVICE_FACTORY_H_
