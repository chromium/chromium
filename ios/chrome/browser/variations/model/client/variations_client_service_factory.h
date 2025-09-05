// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_CLIENT_VARIATIONS_CLIENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_CLIENT_VARIATIONS_CLIENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class VariationsClientService;
class ProfileIOS;

// Factory for VariationsClientService.
class VariationsClientServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static VariationsClientService* GetForProfile(ProfileIOS* profile);
  static VariationsClientServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<VariationsClientServiceFactory>;

  VariationsClientServiceFactory();
  ~VariationsClientServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_CLIENT_VARIATIONS_CLIENT_SERVICE_FACTORY_H_
