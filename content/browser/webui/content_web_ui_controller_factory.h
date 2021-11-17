// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_CONTENT_WEB_UI_CONTROLLER_FACTORY_H_
#define CONTENT_BROWSER_WEBUI_CONTENT_WEB_UI_CONTROLLER_FACTORY_H_

#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"

namespace content {

class CONTENT_EXPORT ContentWebUIControllerFactory
    : public WebUIControllerFactory {
 public:
  static ContentWebUIControllerFactory* GetInstance();

  ContentWebUIControllerFactory(const ContentWebUIControllerFactory&) = delete;
  ContentWebUIControllerFactory& operator=(
      const ContentWebUIControllerFactory&) = delete;

  // WebUIControllerFactory:
  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override;
  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override;
  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override;

 protected:
  ContentWebUIControllerFactory();
  ~ContentWebUIControllerFactory() override;

 private:
  friend struct base::DefaultSingletonTraits<ContentWebUIControllerFactory>;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_CONTENT_WEB_UI_CONTROLLER_FACTORY_H_
