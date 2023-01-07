// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_API_FACTORY_H_
#define EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_API_FACTORY_H_

#include "base/lazy_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace extensions {
class PrinterProviderAPI;
}

namespace extensions {

// Factory for PrinterProviderAPI.
class PrinterProviderAPIFactory : public BrowserContextKeyedServiceFactory {
 public:
  PrinterProviderAPIFactory(const PrinterProviderAPIFactory&) = delete;
  PrinterProviderAPIFactory& operator=(const PrinterProviderAPIFactory&) =
      delete;

  static PrinterProviderAPIFactory* GetInstance();

  PrinterProviderAPI* GetForBrowserContext(content::BrowserContext* context);

 private:
  friend struct base::LazyInstanceTraitsBase<PrinterProviderAPIFactory>;

  PrinterProviderAPIFactory();
  ~PrinterProviderAPIFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_API_FACTORY_H_
