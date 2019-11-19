// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_EVENT_ROUTER_FACTORY_H_
#define EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_EVENT_ROUTER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class DisplaySourceEventRouter;

class DisplaySourceEventRouterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the DisplaySourceEventRouter for |profile|, creating it if
  // it is not yet created.
  static DisplaySourceEventRouter* GetForProfile(
      content::BrowserContext* context);

  static DisplaySourceEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend struct base::DefaultSingletonTraits<DisplaySourceEventRouterFactory>;

  DisplaySourceEventRouterFactory();
  ~DisplaySourceEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(DisplaySourceEventRouterFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_EVENT_ROUTER_FACTORY_H_
