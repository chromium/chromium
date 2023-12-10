// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class MailtoHandlerService;

// Singleton that owns all MailtoHandlerServices and associates them with
// ChromeBrowserState.
class MailtoHandlerServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static MailtoHandlerService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static MailtoHandlerServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<MailtoHandlerServiceFactory>;

  MailtoHandlerServiceFactory();
  ~MailtoHandlerServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_SERVICE_FACTORY_H_
