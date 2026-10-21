// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_SERVICE_FACTORY_H_

#import "components/history/core/browser/history_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all HistoryServices and associates them with
// ProfileIOS.
class HistoryServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<HistoryServiceFactory,
                                                history::HistoryService> {
 public:
  explicit HistoryServiceFactory(PassKey key);

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_SERVICE_FACTORY_H_
