// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

enum class ServiceAccessType;

namespace password_manager {
class BulkLeakCheckServiceInterface;
}

// Singleton that owns all BulkLeakCheckServices and associates them with
// profile.
class IOSChromeBulkLeakCheckServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static password_manager::BulkLeakCheckServiceInterface* GetForBrowserState(
      ProfileIOS* profile);

  static password_manager::BulkLeakCheckServiceInterface* GetForProfile(
      ProfileIOS* profile);
  static IOSChromeBulkLeakCheckServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromeBulkLeakCheckServiceFactory>;

  IOSChromeBulkLeakCheckServiceFactory();
  ~IOSChromeBulkLeakCheckServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
