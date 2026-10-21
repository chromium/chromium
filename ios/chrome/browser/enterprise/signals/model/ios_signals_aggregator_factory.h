// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_IOS_SIGNALS_AGGREGATOR_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_IOS_SIGNALS_AGGREGATOR_FACTORY_H_

#import "components/device_signals/core/browser/signals_aggregator.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class IOSSignalsAggregatorFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSSignalsAggregatorFactory,
          device_signals::SignalsAggregator> {
 public:
  explicit IOSSignalsAggregatorFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_IOS_SIGNALS_AGGREGATOR_FACTORY_H_
