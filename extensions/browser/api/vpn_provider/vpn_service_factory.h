// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_VPN_PROVIDER_VPN_SERVICE_FACTORY_H_
#define EXTENSIONS_BROWSER_API_VPN_PROVIDER_VPN_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {

class BrowserContext;

}  // namespace content

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace chromeos {

class VpnService;

// Factory to create VpnService.
class VpnServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  VpnServiceFactory(const VpnServiceFactory&) = delete;
  VpnServiceFactory& operator=(const VpnServiceFactory&) = delete;

  static VpnService* GetForBrowserContext(content::BrowserContext* context);
  static VpnServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<VpnServiceFactory>;

  VpnServiceFactory();
  ~VpnServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // EXTENSIONS_BROWSER_API_VPN_PROVIDER_VPN_SERVICE_FACTORY_H_
