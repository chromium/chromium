// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROVIDER_STATE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROVIDER_STATE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class KeyedService;
class ProfileIOS;
struct ProviderStateService;

namespace web {
class BrowserState;
}

namespace ios {

// Singleton that owns all ProviderStateServices and associates them with
// profiles.
class ProviderStateServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ProviderStateService* GetForProfile(ProfileIOS* profile);
  static ProviderStateServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ProviderStateServiceFactory>;

  ProviderStateServiceFactory();
  ~ProviderStateServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROVIDER_STATE_SERVICE_FACTORY_H_
