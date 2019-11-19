// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBDATA_SERVICES_WEB_DATA_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEBDATA_SERVICES_WEB_DATA_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class KeywordWebDataService;
class TokenWebData;
class WebDataServiceWrapper;
enum class ServiceAccessType;

namespace autofill {
class AutofillWebDataService;
}

namespace ios {

class ChromeBrowserState;

// Singleton that owns all WebDataServiceWrappers and associates them with
// ios::ChromeBrowserState.
class WebDataServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the AutofillWebDataService associated with |browser_state|.
  static WebDataServiceWrapper* GetForBrowserState(
      ios::ChromeBrowserState* browser_state,
      ServiceAccessType access_type);
  static WebDataServiceWrapper* GetForBrowserStateIfExists(
      ios::ChromeBrowserState* browser_state,
      ServiceAccessType access_type);

  // Returns the AutofillWebDataService associated with |browser_state|.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForBrowserState(ios::ChromeBrowserState* browser_state,
                                    ServiceAccessType access_type);

  // Returns the account-scoped AutofillWebDataService associated with the
  // |browser_state|.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForAccount(ios::ChromeBrowserState* browser_state,
                               ServiceAccessType access_type);

  // Returns the KeywordWebDataService associated with |browser_state|.
  static scoped_refptr<KeywordWebDataService> GetKeywordWebDataForBrowserState(
      ios::ChromeBrowserState* browser_state,
      ServiceAccessType access_type);

  // Returns the TokenWebData associated with |browser_state|.
  static scoped_refptr<TokenWebData> GetTokenWebDataForBrowserState(
      ios::ChromeBrowserState* browser_state,
      ServiceAccessType access_type);

  static WebDataServiceFactory* GetInstance();

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

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_WEBDATA_SERVICES_WEB_DATA_SERVICE_FACTORY_H_
