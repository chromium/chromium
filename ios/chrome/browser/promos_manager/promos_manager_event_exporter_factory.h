// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_EVENT_EXPORTER_FACTORY_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_EVENT_EXPORTER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class PromosManagerEventExporter;

// Singleton that owns all PromosManagerEventExporters and associates them with
// ChromeBrowserState.
class PromosManagerEventExporterFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static PromosManagerEventExporter* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static PromosManagerEventExporterFactory* GetInstance();

  PromosManagerEventExporterFactory(const PromosManagerEventExporterFactory&) =
      delete;
  PromosManagerEventExporterFactory& operator=(
      const PromosManagerEventExporterFactory&) = delete;

 private:
  friend class base::NoDestructor<PromosManagerEventExporterFactory>;

  PromosManagerEventExporterFactory();
  ~PromosManagerEventExporterFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* state) const override;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_EVENT_EXPORTER_FACTORY_H_
