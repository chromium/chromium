// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_PERMISSIONS_PERMISSIONS_EVENT_ROUTER_FACTORY_H_
#define EXTENSIONS_BROWSER_API_PERMISSIONS_PERMISSIONS_EVENT_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

// The factory responsible for creating the event router for the permissions
// API.
class PermissionsEventRouterFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the PermissionsEventRouterFactory instance.
  static PermissionsEventRouterFactory* GetInstance();

  PermissionsEventRouterFactory(const PermissionsEventRouterFactory&) = delete;
  PermissionsEventRouterFactory& operator=(
      const PermissionsEventRouterFactory&) = delete;

 private:
  friend base::NoDestructor<PermissionsEventRouterFactory>;

  PermissionsEventRouterFactory();
  ~PermissionsEventRouterFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PERMISSIONS_PERMISSIONS_EVENT_ROUTER_FACTORY_H_
