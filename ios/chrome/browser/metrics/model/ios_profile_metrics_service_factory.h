// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_METRICS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_METRICS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace metrics {
class ProfileMetricsService;
}

// iOS factory for creating ProfileMetricsService.
class IOSProfileMetricsServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static metrics::ProfileMetricsService* GetForProfile(ProfileIOS* profile);

  static IOSProfileMetricsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSProfileMetricsServiceFactory>;

  IOSProfileMetricsServiceFactory();
  ~IOSProfileMetricsServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_METRICS_SERVICE_FACTORY_H_
