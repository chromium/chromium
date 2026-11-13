// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_HISTORY_OBSERVER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_HISTORY_OBSERVER_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/safe_browsing/model/suspicious_site_history_observer_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all `SuspiciousSiteHistoryObserverService` instances and
// associates them with a `ProfileIOS`.
class SuspiciousSiteHistoryObserverServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          SuspiciousSiteHistoryObserverServiceFactory,
          SuspiciousSiteHistoryObserverService> {
 public:
  // Constructs the factory singleton.
  explicit SuspiciousSiteHistoryObserverServiceFactory(PassKey key);

  // Returns the default factory. Can be used to force instantiation during
  // testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS& profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_HISTORY_OBSERVER_SERVICE_FACTORY_H_
