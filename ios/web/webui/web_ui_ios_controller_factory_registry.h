// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_WEB_UI_IOS_CONTROLLER_FACTORY_REGISTRY_H_
#define IOS_WEB_WEBUI_WEB_UI_IOS_CONTROLLER_FACTORY_REGISTRY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"

namespace web {

// A singleton which holds on to all the registered WebUIIOSControllerFactory
// instances.
class WebUIIOSControllerFactoryRegistry : public WebUIIOSControllerFactory {
 public:
  static WebUIIOSControllerFactoryRegistry* GetInstance();

  NSInteger GetErrorCodeForWebUIURL(const GURL& url) const override;

  // WebUIIOSControllerFactory implementation. Each method loops through the
  // same method on all the factories.
  std::unique_ptr<WebUIIOSController> CreateWebUIIOSControllerForURL(
      WebUIIOS* web_ui,
      const GURL& url) const override;

 private:
  friend class base::NoDestructor<WebUIIOSControllerFactoryRegistry>;

  WebUIIOSControllerFactoryRegistry();
  ~WebUIIOSControllerFactoryRegistry() override;

  DISALLOW_COPY_AND_ASSIGN(WebUIIOSControllerFactoryRegistry);
};

}  // namespace web

#endif  // IOS_WEB_WEBUI_WEB_UI_IOS_CONTROLLER_FACTORY_REGISTRY_H_
