// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_MODEL_WEB_HISTORY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_MODEL_WEB_HISTORY_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace history {
class WebHistoryService;
}

namespace ios {
// Singleton that owns all WebHistoryServices and associates them with
// profiles.
class WebHistoryServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static history::WebHistoryService* GetForProfile(ProfileIOS* profile);
  static WebHistoryServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebHistoryServiceFactory>;

  WebHistoryServiceFactory();
  ~WebHistoryServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_HISTORY_MODEL_WEB_HISTORY_SERVICE_FACTORY_H_
