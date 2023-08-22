// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_FACTORY_H_
#define EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class UpdateService;

// Service factory to construct UpdateService instances per BrowserContext.
// Note that OTR browser contexts do not get an UpdateService.
class UpdateServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  UpdateServiceFactory(const UpdateServiceFactory&) = delete;
  UpdateServiceFactory& operator=(const UpdateServiceFactory&) = delete;

  static UpdateService* GetForBrowserContext(content::BrowserContext* context);
  static UpdateServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<UpdateServiceFactory>;

  UpdateServiceFactory();
  ~UpdateServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_FACTORY_H_
