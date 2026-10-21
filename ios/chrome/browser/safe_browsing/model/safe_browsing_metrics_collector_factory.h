// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_

#import "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Used to construct a SafeBrowsingMetricsCollector. Returns null for
// incognito profiles.
class SafeBrowsingMetricsCollectorFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          SafeBrowsingMetricsCollectorFactory,
          safe_browsing::SafeBrowsingMetricsCollector> {
 public:
  explicit SafeBrowsingMetricsCollectorFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_
