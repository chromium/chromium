// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_autofill_image_fetcher_factory.h"

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "components/autofill/core/browser/ui/autofill_image_fetcher.h"
#import "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/web_view/internal/autofill/web_view_autofill_image_fetcher_impl.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

namespace ios_web_view {

// static
WebViewAutofillImageFetcherImpl*
WebViewAutofillImageFetcherFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<WebViewAutofillImageFetcherImpl*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
WebViewAutofillImageFetcherFactory*
WebViewAutofillImageFetcherFactory::GetInstance() {
  static base::NoDestructor<WebViewAutofillImageFetcherFactory> instance;
  return instance.get();
}

WebViewAutofillImageFetcherFactory::WebViewAutofillImageFetcherFactory()
    : BrowserStateKeyedServiceFactory(
          "AutofillImageFetcher",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewAutofillImageFetcherFactory::~WebViewAutofillImageFetcherFactory() =
    default;

std::unique_ptr<KeyedService>
WebViewAutofillImageFetcherFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<ios_web_view::WebViewAutofillImageFetcherImpl>(
      context->GetSharedURLLoaderFactory());
}

}  // namespace ios_web_view
