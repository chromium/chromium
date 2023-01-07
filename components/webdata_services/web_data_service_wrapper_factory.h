// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_SERVICES_WEB_DATA_SERVICE_WRAPPER_FACTORY_H_
#define COMPONENTS_WEBDATA_SERVICES_WEB_DATA_SERVICE_WRAPPER_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace payments {
class PaymentManifestWebDataService;
}  // namespace payments

class WebDataServiceWrapper;

namespace webdata_services {

// A factory that lazily returns a WebDataServiceWrapper implementation for a
// given BrowserContext.
class WebDataServiceWrapperFactory : public BrowserContextKeyedServiceFactory {
 public:
  WebDataServiceWrapperFactory(const WebDataServiceWrapperFactory&) = delete;
  WebDataServiceWrapperFactory& operator=(const WebDataServiceWrapperFactory&) =
      delete;

  static WebDataServiceWrapper* GetForBrowserContext(
      content::BrowserContext* context,
      ServiceAccessType access_type);

  static WebDataServiceWrapper* GetForBrowserContextIfExists(
      content::BrowserContext* context,
      ServiceAccessType access_type);

  static scoped_refptr<payments::PaymentManifestWebDataService>
  GetPaymentManifestWebDataServiceForBrowserContext(
      content::BrowserContext* context,
      ServiceAccessType access_type);

  static WebDataServiceWrapperFactory* GetInstance();

 protected:
  WebDataServiceWrapperFactory();
  ~WebDataServiceWrapperFactory() override;
};

}  // namespace webdata_services

#endif  // COMPONENTS_WEBDATA_SERVICES_WEB_DATA_SERVICE_WRAPPER_FACTORY_H_
