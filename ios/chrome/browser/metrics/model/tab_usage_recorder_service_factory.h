// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_SERVICE_FACTORY_H_

#include "base/types/pass_key.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class TabUsageRecorderService;

// Factory associating TabUsageRecorderService instance to ProfileIOS.
class TabUsageRecorderServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Prevents construction.
  using PassKey = base::PassKey<TabUsageRecorderServiceFactory>;

  static TabUsageRecorderService* GetForProfile(ProfileIOS*);
  static TabUsageRecorderServiceFactory* GetInstance();

  TabUsageRecorderServiceFactory(PassKey);

 private:
  ~TabUsageRecorderServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_SERVICE_FACTORY_H_
