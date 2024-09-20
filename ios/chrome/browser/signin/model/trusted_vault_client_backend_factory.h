// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class TrustedVaultClientBackend;

// Singleton that owns all TrustedVaultClientBackends and associates them with
// ChromeBrowserState.
class TrustedVaultClientBackendFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static TrustedVaultClientBackend* GetForBrowserState(ProfileIOS* profile);

  static TrustedVaultClientBackend* GetForProfile(ProfileIOS* profile);
  static TrustedVaultClientBackendFactory* GetInstance();

 private:
  friend class base::NoDestructor<TrustedVaultClientBackendFactory>;

  TrustedVaultClientBackendFactory();
  ~TrustedVaultClientBackendFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_FACTORY_H_
