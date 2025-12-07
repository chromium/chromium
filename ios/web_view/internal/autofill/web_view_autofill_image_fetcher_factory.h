// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_IMAGE_FETCHER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace ios_web_view {

class WebViewAutofillImageFetcherImpl;

// Class that owns all AutofillImageFetchers and associates them with
// BrowserState.
class WebViewAutofillImageFetcherFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static WebViewAutofillImageFetcherImpl* GetForBrowserState(
      web::BrowserState* browser_state);

  static WebViewAutofillImageFetcherFactory* GetInstance();

  WebViewAutofillImageFetcherFactory(
      const WebViewAutofillImageFetcherFactory&) = delete;
  WebViewAutofillImageFetcherFactory& operator=(
      const WebViewAutofillImageFetcherFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewAutofillImageFetcherFactory>;

  WebViewAutofillImageFetcherFactory();
  ~WebViewAutofillImageFetcherFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
