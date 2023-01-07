// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata_services/web_data_service_wrapper_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "content/public/browser/browser_context.h"

namespace webdata_services {

namespace {
WebDataServiceWrapperFactory* g_instance = nullptr;
}  // namespace

// static
WebDataServiceWrapper* WebDataServiceWrapperFactory::GetForBrowserContext(
    content::BrowserContext* context,
    ServiceAccessType access_type) {
  DCHECK(context);
  DCHECK(access_type != ServiceAccessType::IMPLICIT_ACCESS ||
         !context->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
WebDataServiceWrapper*
WebDataServiceWrapperFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context,
    ServiceAccessType access_type) {
  DCHECK(context);
  DCHECK(access_type != ServiceAccessType::IMPLICIT_ACCESS ||
         !context->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/false));
}

// static
scoped_refptr<payments::PaymentManifestWebDataService>
WebDataServiceWrapperFactory::GetPaymentManifestWebDataServiceForBrowserContext(
    content::BrowserContext* context,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper = GetForBrowserContext(context, access_type);
  return wrapper ? wrapper->GetPaymentManifestWebData() : nullptr;
}

// static
WebDataServiceWrapperFactory* WebDataServiceWrapperFactory::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

WebDataServiceWrapperFactory::WebDataServiceWrapperFactory()
    : BrowserContextKeyedServiceFactory(
          "WebDataService",
          BrowserContextDependencyManager::GetInstance()) {
  DCHECK(!g_instance);
  g_instance = this;
}

WebDataServiceWrapperFactory::~WebDataServiceWrapperFactory() {
  g_instance = nullptr;
}

}  // namespace webdata_services
