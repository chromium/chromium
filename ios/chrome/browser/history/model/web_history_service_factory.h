// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_MODEL_WEB_HISTORY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_MODEL_WEB_HISTORY_SERVICE_FACTORY_H_

#import "components/history/core/browser/web_history_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all WebHistoryServices and associates them with
// profiles.
class WebHistoryServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<WebHistoryServiceFactory,
                                                history::WebHistoryService> {
 public:
  explicit WebHistoryServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_HISTORY_MODEL_WEB_HISTORY_SERVICE_FACTORY_H_
