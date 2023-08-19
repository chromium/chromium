// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/webui/web_view_web_ui_ios_controller_factory.h"

#import <Foundation/Foundation.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "ios/web_view/internal/webui/web_view_sync_internals_ui.h"
#include "url/gurl.h"

using web::WebUIIOS;
using web::WebUIIOSController;

namespace ios_web_view {

namespace {

// A function for creating a new WebUIIOS.
using WebUIIOSFactoryFunction =
    std::unique_ptr<WebUIIOSController> (*)(WebUIIOS* web_ui, const GURL& url);

// Template for defining WebUIIOSFactoryFunction.
template <class T>
std::unique_ptr<WebUIIOSController> NewWebUIIOS(WebUIIOS* web_ui,
                                                const GURL& url) {
  return std::make_unique<T>(web_ui, url.host());
}

// Returns a function that can be used to create the right type of WebUIIOS for
// a tab, based on its URL. Returns nullptr if the URL doesn't have WebUIIOS
// associated with it.
WebUIIOSFactoryFunction GetWebUIIOSFactoryFunction(const GURL& url) {
  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes to filter out most URLs.
  if (!url.SchemeIs(kChromeUIScheme))
    return nullptr;

  // Please keep this in alphabetical order. If #ifs or special logic is
  // required, add it below in the appropriate section.
  const std::string url_host = url.host();
  if (url_host == kChromeUISyncInternalsHost)
    return &NewWebUIIOS<WebViewSyncInternalsUI>;

  return nullptr;
}

}  // namespace

NSInteger WebViewWebUIIOSControllerFactory::GetErrorCodeForWebUIURL(
    const GURL& url) const {
  if (GetWebUIIOSFactoryFunction(url))
    return 0;
  return NSURLErrorUnsupportedURL;
}

std::unique_ptr<WebUIIOSController>
WebViewWebUIIOSControllerFactory::CreateWebUIIOSControllerForURL(
    WebUIIOS* web_ui,
    const GURL& url) const {
  WebUIIOSFactoryFunction function = GetWebUIIOSFactoryFunction(url);
  if (!function)
    return nullptr;

  return (*function)(web_ui, url);
}

// static
WebViewWebUIIOSControllerFactory*
WebViewWebUIIOSControllerFactory::GetInstance() {
  static base::NoDestructor<WebViewWebUIIOSControllerFactory> instance;
  return instance.get();
}

WebViewWebUIIOSControllerFactory::WebViewWebUIIOSControllerFactory() {}

WebViewWebUIIOSControllerFactory::~WebViewWebUIIOSControllerFactory() {}

}  // namespace ios_web_view
