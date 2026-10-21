// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_METRICS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_METRICS_SERVICE_FACTORY_H_

#import "components/metrics/profile_metrics_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// iOS factory for creating ProfileMetricsService.
class IOSProfileMetricsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSProfileMetricsServiceFactory,
          metrics::ProfileMetricsService> {
 public:
  explicit IOSProfileMetricsServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_METRICS_SERVICE_FACTORY_H_
