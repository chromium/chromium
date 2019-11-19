// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_IDLE_IDLE_MANAGER_FACTORY_H__
#define EXTENSIONS_BROWSER_API_IDLE_IDLE_MANAGER_FACTORY_H__

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class IdleManager;

class IdleManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static IdleManager* GetForBrowserContext(content::BrowserContext* context);

  static IdleManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<IdleManagerFactory>;

  IdleManagerFactory();
  ~IdleManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_IDLE_IDLE_MANAGER_FACTORY_H__
