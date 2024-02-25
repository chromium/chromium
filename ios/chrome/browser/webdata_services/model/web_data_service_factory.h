// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBDATA_SERVICES_MODEL_WEB_DATA_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEBDATA_SERVICES_MODEL_WEB_DATA_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class KeywordWebDataService;
class TokenWebData;
class WebDataServiceWrapper;
enum class ServiceAccessType;

namespace autofill {
class AutofillWebDataService;
}

namespace ios {
// Singleton that owns all WebDataServiceWrappers and associates them with
// ChromeBrowserState.
class WebDataServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the AutofillWebDataService associated with `browser_state`.
  static WebDataServiceWrapper* GetForBrowserState(
      ChromeBrowserState* browser_state,
      ServiceAccessType access_type);
  static WebDataServiceWrapper* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state,
      ServiceAccessType access_type);

  // Returns the AutofillWebDataService associated with `browser_state`.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForBrowserState(ChromeBrowserState* browser_state,
                                    ServiceAccessType access_type);

  // Returns the account-scoped AutofillWebDataService associated with the
  // `browser_state`.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForAccount(ChromeBrowserState* browser_state,
                               ServiceAccessType access_type);

  // Returns the KeywordWebDataService associated with `browser_state`.
  static scoped_refptr<KeywordWebDataService> GetKeywordWebDataForBrowserState(
      ChromeBrowserState* browser_state,
      ServiceAccessType access_type);

  // Returns the TokenWebData associated with `browser_state`.
  static scoped_refptr<TokenWebData> GetTokenWebDataForBrowserState(
      ChromeBrowserState* browser_state,
      ServiceAccessType access_type);

  static WebDataServiceFactory* GetInstance();

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

  WebDataServiceFactory(const WebDataServiceFactory&) = delete;
  WebDataServiceFactory& operator=(const WebDataServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<WebDataServiceFactory>;

  WebDataServiceFactory();
  ~WebDataServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_WEBDATA_SERVICES_MODEL_WEB_DATA_SERVICE_FACTORY_H_
