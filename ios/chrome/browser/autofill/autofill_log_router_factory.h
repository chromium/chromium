// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_LOG_ROUTER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_LOG_ROUTER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace ios {
class ChromeBrowserState;
}

namespace autofill {

class LogRouter;

// A factory that associates autofill::LogRouter instances with
// ChromeBrowserStates. This returns nullptr of off-the-record browser states.
class AutofillLogRouterFactory : public BrowserStateKeyedServiceFactory {
 public:
  static autofill::LogRouter* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static AutofillLogRouterFactory* GetInstance();

 private:
  friend class base::NoDestructor<AutofillLogRouterFactory>;

  AutofillLogRouterFactory();
  ~AutofillLogRouterFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AutofillLogRouterFactory);
};

}  // namespace autofill
#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_LOG_ROUTER_FACTORY_H_
