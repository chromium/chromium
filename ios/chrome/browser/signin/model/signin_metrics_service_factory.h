// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_METRICS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_METRICS_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class SigninMetricsService;

// Singleton that manages the `SigninMetricsService` service per profile.
class SigninMetricsServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static SigninMetricsService* GetForProfile(ProfileIOS* profile);
  static SigninMetricsServiceFactory* GetInstance();

  SigninMetricsServiceFactory(const SigninMetricsServiceFactory&) = delete;
  SigninMetricsServiceFactory& operator=(const SigninMetricsServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<SigninMetricsServiceFactory>;

  SigninMetricsServiceFactory();
  ~SigninMetricsServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_METRICS_SERVICE_FACTORY_H_
