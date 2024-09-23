// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class SharingService;

// Singleton that owns all SharingService and associates them with
// ProfileIOS.
class IOSSharingServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static SharingService* GetForProfile(ProfileIOS* profile);
  static SharingService* GetForProfileIfExists(ProfileIOS* profile);
  static IOSSharingServiceFactory* GetInstance();

  IOSSharingServiceFactory(const IOSSharingServiceFactory&) = delete;
  IOSSharingServiceFactory& operator=(const IOSSharingServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSSharingServiceFactory>;

  IOSSharingServiceFactory();
  ~IOSSharingServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
};

#endif  // IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_SERVICE_FACTORY_H_
