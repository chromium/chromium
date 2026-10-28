// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRIVATE_INSIGHTS_MODEL_PRIVATE_INSIGHTS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PRIVATE_INSIGHTS_MODEL_PRIVATE_INSIGHTS_SERVICE_FACTORY_H_

#import <memory>

#import "components/private_insights/private_insights_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Gives each regular profile a `PrivateInsightsService`, which logs events and
// uploads them for privacy-preserving aggregation through Federated Compute.
class PrivateInsightsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          PrivateInsightsServiceFactory,
          private_insights::PrivateInsightsService> {
 public:
  explicit PrivateInsightsServiceFactory(PassKey key);

  // Returns the default factory used to build `PrivateInsightsService`. Can be
  // registered with `AddTestingFactory` to use real instances during testing.
  static TestingFactoryByRef GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS& profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PRIVATE_INSIGHTS_MODEL_PRIVATE_INSIGHTS_SERVICE_FACTORY_H_
