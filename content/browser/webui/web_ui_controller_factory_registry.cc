// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_controller_factory_registry.h"

#include <stddef.h>

#include "base/lazy_instance.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "url/gurl.h"

namespace content {

base::LazyInstance<std::vector<WebUIControllerFactory*>>::DestructorAtExit
    g_web_ui_controller_factories = LAZY_INSTANCE_INITIALIZER;

void WebUIControllerFactory::RegisterFactory(WebUIControllerFactory* factory) {
  g_web_ui_controller_factories.Pointer()->push_back(factory);
}

void WebUIControllerFactory::UnregisterFactoryForTesting(
    WebUIControllerFactory* factory) {
  std::vector<WebUIControllerFactory*>* factories =
      g_web_ui_controller_factories.Pointer();
  for (size_t i = 0; i < factories->size(); ++i) {
    if ((*factories)[i] == factory) {
      factories->erase(factories->begin() + i);
      return;
    }
  }
  NOTREACHED() << "Tried to unregister a factory but it wasn't found";
}

WebUIControllerFactoryRegistry* WebUIControllerFactoryRegistry::GetInstance() {
  return base::Singleton<WebUIControllerFactoryRegistry>::get();
}

std::unique_ptr<WebUIController>
WebUIControllerFactoryRegistry::CreateWebUIControllerForURL(WebUI* web_ui,
                                                            const GURL& url) {
  std::vector<WebUIControllerFactory*>& factories =
      g_web_ui_controller_factories.Get();
  for (WebUIControllerFactory* factory : factories) {
    auto controller = factory->CreateWebUIControllerForURL(web_ui, url);
    if (controller)
      return controller;
  }
  return nullptr;
}

WebUI::TypeID WebUIControllerFactoryRegistry::GetWebUIType(
    BrowserContext* browser_context,
    const GURL& url) {
  std::vector<WebUIControllerFactory*>* factories =
      g_web_ui_controller_factories.Pointer();
  for (size_t i = 0; i < factories->size(); ++i) {
    WebUI::TypeID type = (*factories)[i]->GetWebUIType(browser_context, url);
    if (type != WebUI::kNoWebUI)
      return type;
  }
  return WebUI::kNoWebUI;
}

bool WebUIControllerFactoryRegistry::UseWebUIForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  std::vector<WebUIControllerFactory*>* factories =
      g_web_ui_controller_factories.Pointer();
  for (size_t i = 0; i < factories->size(); ++i) {
    if ((*factories)[i]->UseWebUIForURL(browser_context, url))
      return true;
  }
  return false;
}

bool WebUIControllerFactoryRegistry::UseWebUIBindingsForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  std::vector<WebUIControllerFactory*>* factories =
      g_web_ui_controller_factories.Pointer();
  for (size_t i = 0; i < factories->size(); ++i) {
    if ((*factories)[i]->UseWebUIBindingsForURL(browser_context, url))
      return true;
  }
  return false;
}

bool WebUIControllerFactoryRegistry::IsURLAcceptableForWebUI(
    BrowserContext* browser_context,
    const GURL& url) {
  return UseWebUIForURL(browser_context, url) ||
         // It's possible to load about:blank in a Web UI renderer.
         // See http://crbug.com/42547
         url.spec() == url::kAboutBlankURL ||
         // javascript: and debug URLs like chrome://kill are allowed.
         IsRendererDebugURL(url) ||
         // Temporarily allow the embedder to whitelist URLs allowed in WebUI
         // until crbug.com/768526 is resolved.
         GetContentClient()->browser()->IsURLAcceptableForWebUI(browser_context,
                                                                url);
}

WebUIControllerFactoryRegistry::WebUIControllerFactoryRegistry() {
}

WebUIControllerFactoryRegistry::~WebUIControllerFactoryRegistry() {
}

}  // namespace content
