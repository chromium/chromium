// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/content_web_ui_controller_factory.h"

#include "content/public/browser/web_ui.h"

namespace content {

WebUI::TypeID ContentWebUIControllerFactory::GetWebUIType(
    BrowserContext* browser_context,
    const GURL& url) {
  return WebUI::kNoWebUI;
}

bool ContentWebUIControllerFactory::UseWebUIForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

std::unique_ptr<WebUIController>
ContentWebUIControllerFactory::CreateWebUIControllerForURL(WebUI* web_ui,
                                                           const GURL& url) {
  return nullptr;
}

// static
ContentWebUIControllerFactory* ContentWebUIControllerFactory::GetInstance() {
  return base::Singleton<ContentWebUIControllerFactory>::get();
}

ContentWebUIControllerFactory::ContentWebUIControllerFactory() {
}

ContentWebUIControllerFactory::~ContentWebUIControllerFactory() {
}

}  // namespace content
