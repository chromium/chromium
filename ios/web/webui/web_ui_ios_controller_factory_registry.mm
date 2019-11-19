// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/webui/web_ui_ios_controller_factory_registry.h"

#include <stddef.h>
#include <memory>

#include "base/no_destructor.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {
// Returns the global list of registered factories.
std::vector<WebUIIOSControllerFactory*>& GetGlobalFactories() {
  static base::NoDestructor<std::vector<WebUIIOSControllerFactory*>> factories;
  return *factories;
}
}  // namespace

void WebUIIOSControllerFactory::RegisterFactory(
    WebUIIOSControllerFactory* factory) {
  GetGlobalFactories().push_back(factory);
}

void WebUIIOSControllerFactory::DeregisterFactory(
    WebUIIOSControllerFactory* factory) {
  std::vector<WebUIIOSControllerFactory*>& factories = GetGlobalFactories();
  auto position = std::find(factories.begin(), factories.end(), factory);
  if (position != factories.end())
    factories.erase(position);
}

WebUIIOSControllerFactoryRegistry*
WebUIIOSControllerFactoryRegistry::GetInstance() {
  static base::NoDestructor<WebUIIOSControllerFactoryRegistry> instance;
  return instance.get();
}

NSInteger WebUIIOSControllerFactoryRegistry::GetErrorCodeForWebUIURL(
    const GURL& url) const {
  NSInteger error_code = NSURLErrorUnsupportedURL;
  for (WebUIIOSControllerFactory* factory : GetGlobalFactories()) {
    error_code = factory->GetErrorCodeForWebUIURL(url);
    if (error_code == 0)
      return 0;
  }
  return error_code;
}

std::unique_ptr<WebUIIOSController>
WebUIIOSControllerFactoryRegistry::CreateWebUIIOSControllerForURL(
    WebUIIOS* web_ui,
    const GURL& url) const {
  for (WebUIIOSControllerFactory* factory : GetGlobalFactories()) {
    auto controller = factory->CreateWebUIIOSControllerForURL(web_ui, url);
    if (controller)
      return controller;
  }
  return nullptr;
}

WebUIIOSControllerFactoryRegistry::WebUIIOSControllerFactoryRegistry() {}

WebUIIOSControllerFactoryRegistry::~WebUIIOSControllerFactoryRegistry() {}

}  // namespace web
