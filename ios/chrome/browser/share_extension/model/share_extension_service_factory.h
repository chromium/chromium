// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ShareExtensionService;

// Singleton that creates the ShareExtensionService and associates that service
// with ProfileIOS.
class ShareExtensionServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static ShareExtensionService* GetForProfile(ProfileIOS* profile);
  static ShareExtensionService* GetForProfileIfExists(ProfileIOS* profile);
  static ShareExtensionServiceFactory* GetInstance();

  ShareExtensionServiceFactory(const ShareExtensionServiceFactory&) = delete;
  ShareExtensionServiceFactory& operator=(const ShareExtensionServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<ShareExtensionServiceFactory>;

  ShareExtensionServiceFactory();
  ~ShareExtensionServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_SERVICE_FACTORY_H_
