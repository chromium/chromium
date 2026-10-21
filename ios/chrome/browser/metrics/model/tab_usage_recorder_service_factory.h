// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_SERVICE_FACTORY_H_

#include "base/types/pass_key.h"
#include "ios/chrome/browser/metrics/model/tab_usage_recorder_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory associating TabUsageRecorderService instance to ProfileIOS.
class TabUsageRecorderServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<TabUsageRecorderServiceFactory,
                                                TabUsageRecorderService> {
 public:
  explicit TabUsageRecorderServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_SERVICE_FACTORY_H_
