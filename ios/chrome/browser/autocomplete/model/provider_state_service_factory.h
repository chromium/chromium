// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROVIDER_STATE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROVIDER_STATE_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class KeyedService;
struct ProviderStateService;

namespace web {
class BrowserState;
}

namespace ios {

// Singleton that owns all ProviderStateServices and associates them with
// profiles.
class ProviderStateServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static ProviderStateService* GetForBrowserState(ProfileIOS* profile);

  static ProviderStateService* GetForProfile(ProfileIOS* profile);
  static ProviderStateServiceFactory* GetInstance();

  ProviderStateServiceFactory(const ProviderStateServiceFactory&) = delete;
  ProviderStateServiceFactory& operator=(const ProviderStateServiceFactory&) =
      delete;

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
