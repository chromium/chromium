// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/web/public/browser_state.h"

class ChromeBrowserState;

namespace trusted_vault {
class TrustedVaultService;
}  // namespace trusted_vault

class IOSTrustedVaultServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static trusted_vault::TrustedVaultService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static IOSTrustedVaultServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSTrustedVaultServiceFactory>;

  IOSTrustedVaultServiceFactory();
  ~IOSTrustedVaultServiceFactory() override;

  // BrowserStateKeyedServiceFactory.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif
