// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace password_manager {
class BulkLeakCheckServiceInterface;
}

// Singleton that owns all BulkLeakCheckServices and associates them with
// profile.
class IOSChromeBulkLeakCheckServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static password_manager::BulkLeakCheckServiceInterface* GetForProfile(
      ProfileIOS* profile);
  static IOSChromeBulkLeakCheckServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromeBulkLeakCheckServiceFactory>;

  IOSChromeBulkLeakCheckServiceFactory();
  ~IOSChromeBulkLeakCheckServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
