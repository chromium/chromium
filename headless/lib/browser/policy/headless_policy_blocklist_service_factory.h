// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_POLICY_HEADLESS_POLICY_BLOCKLIST_SERVICE_FACTORY_H_
#define HEADLESS_LIB_BROWSER_POLICY_HEADLESS_POLICY_BLOCKLIST_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class PolicyBlocklistService;

namespace headless {

// Factory for PolicyBlocklistService in headless mode.
class HeadlessPolicyBlocklistServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static HeadlessPolicyBlocklistServiceFactory* GetInstance();

  static PolicyBlocklistService* GetForBrowserContext(
      content::BrowserContext* context);

  HeadlessPolicyBlocklistServiceFactory(
      const HeadlessPolicyBlocklistServiceFactory&) = delete;
  HeadlessPolicyBlocklistServiceFactory& operator=(
      const HeadlessPolicyBlocklistServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<HeadlessPolicyBlocklistServiceFactory>;

  HeadlessPolicyBlocklistServiceFactory();
  ~HeadlessPolicyBlocklistServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // Finds which browser context (if any) to use.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_POLICY_HEADLESS_POLICY_BLOCKLIST_SERVICE_FACTORY_H_
