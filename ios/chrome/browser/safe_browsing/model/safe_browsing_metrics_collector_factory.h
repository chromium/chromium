// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace safe_browsing {
class SafeBrowsingMetricsCollector;
}

// Used to construct a SafeBrowsingMetricsCollector. Returns null for
// incognito profiles.
class SafeBrowsingMetricsCollectorFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns null if `profile` is in Incognito mode.
  static safe_browsing::SafeBrowsingMetricsCollector* GetForProfile(
      ProfileIOS* profile);
  static SafeBrowsingMetricsCollectorFactory* GetInstance();

 private:
  friend class base::NoDestructor<SafeBrowsingMetricsCollectorFactory>;

  SafeBrowsingMetricsCollectorFactory();
  ~SafeBrowsingMetricsCollectorFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_
