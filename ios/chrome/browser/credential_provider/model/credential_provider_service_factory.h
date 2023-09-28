// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class CredentialProviderService;

// Singleton that owns all CredentialProviderServices and associates them with
// ChromeBrowserState.
class CredentialProviderServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static CredentialProviderService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static CredentialProviderServiceFactory* GetInstance();

  CredentialProviderServiceFactory(const CredentialProviderServiceFactory&) =
      delete;
  CredentialProviderServiceFactory& operator=(
      const CredentialProviderServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<CredentialProviderServiceFactory>;

  CredentialProviderServiceFactory();
  ~CredentialProviderServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_
