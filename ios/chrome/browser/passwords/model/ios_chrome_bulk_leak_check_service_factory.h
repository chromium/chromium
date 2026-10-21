// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all BulkLeakCheckServices and associates them with
// profile.
class IOSChromeBulkLeakCheckServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeBulkLeakCheckServiceFactory,
          password_manager::BulkLeakCheckServiceInterface> {
 public:
  explicit IOSChromeBulkLeakCheckServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
