// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class CredentialProviderService;

// Singleton that owns all CredentialProviderServices and associates them with
// profiles.
class CredentialProviderServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static CredentialProviderService* GetForBrowserState(ProfileIOS* profile);

  static CredentialProviderService* GetForProfile(ProfileIOS* profile);
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
