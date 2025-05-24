// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_METRICS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_METRICS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace supervised_user {
class SupervisedUserMetricsService;
}  // namespace supervised_user

// Singleton that owns SupervisedUserMetricsService object and associates
// them with ProfileIOS.
class SupervisedUserMetricsServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static supervised_user::SupervisedUserMetricsService* GetForProfile(
      ProfileIOS* profile);
  static SupervisedUserMetricsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SupervisedUserMetricsServiceFactory>;

  SupervisedUserMetricsServiceFactory();
  ~SupervisedUserMetricsServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_METRICS_SERVICE_FACTORY_H_
