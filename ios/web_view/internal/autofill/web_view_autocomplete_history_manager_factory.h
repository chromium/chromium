// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace autofill {
class AutocompleteHistoryManager;
}

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all AutocompleteHistoryManagers and associates them with
// ios_web_view::WebViewBrowserState.
class WebViewAutocompleteHistoryManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static autofill::AutocompleteHistoryManager* GetForBrowserState(
      WebViewBrowserState* browser_state);
  static WebViewAutocompleteHistoryManagerFactory* GetInstance();

  WebViewAutocompleteHistoryManagerFactory(
      const WebViewAutocompleteHistoryManagerFactory&) = delete;
  WebViewAutocompleteHistoryManagerFactory& operator=(
      const WebViewAutocompleteHistoryManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewAutocompleteHistoryManagerFactory>;

  WebViewAutocompleteHistoryManagerFactory();
  ~WebViewAutocompleteHistoryManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_
