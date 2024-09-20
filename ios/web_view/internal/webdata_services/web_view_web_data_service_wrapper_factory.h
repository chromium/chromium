// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEBDATA_SERVICES_WEB_VIEW_WEB_DATA_SERVICE_WRAPPER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_WEBDATA_SERVICES_WEB_VIEW_WEB_DATA_SERVICE_WRAPPER_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class TokenWebData;
class WebDataServiceWrapper;
enum class ServiceAccessType;

namespace autofill {
class AutofillWebDataService;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all WebDataServiceWrappers and associates them with
// a browser state.
class WebViewWebDataServiceWrapperFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the AutofillWebDataService associated with |browser_state|.
  static WebDataServiceWrapper* GetForBrowserState(
      WebViewBrowserState* browser_state,
      ServiceAccessType access_type);
  static WebDataServiceWrapper* GetForProfileIfExists(
      WebViewBrowserState* browser_state,
      ServiceAccessType access_type);

  // Returns the AutofillWebDataService associated with |browser_state|.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForBrowserState(WebViewBrowserState* browser_state,
                                    ServiceAccessType access_type);

  // Returns the account-scoped AutofillWebDataService associated with the
  // |browser_state|.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForAccount(WebViewBrowserState* browser_state,
                               ServiceAccessType access_type);

  // Returns the TokenWebData associated with |browser_state|.
  static scoped_refptr<TokenWebData> GetTokenWebDataForBrowserState(
      WebViewBrowserState* browser_state,
      ServiceAccessType access_type);

  static WebViewWebDataServiceWrapperFactory* GetInstance();

  WebViewWebDataServiceWrapperFactory(
      const WebViewWebDataServiceWrapperFactory&) = delete;
  WebViewWebDataServiceWrapperFactory& operator=(
      const WebViewWebDataServiceWrapperFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewWebDataServiceWrapperFactory>;

  WebViewWebDataServiceWrapperFactory();
  ~WebViewWebDataServiceWrapperFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEBDATA_SERVICES_WEB_VIEW_WEB_DATA_SERVICE_WRAPPER_FACTORY_H_
