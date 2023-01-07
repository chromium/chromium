// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEBUI_WEB_VIEW_WEB_UI_IOS_CONTROLLER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_WEBUI_WEB_VIEW_WEB_UI_IOS_CONTROLLER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"

class GURL;

namespace ios_web_view {

// Returns WebUIIOSControllers for supported WebUI URLs in //ios/web_view.
// Unlike native UI implemented with UIKit, WebUI is UI implemented using web
// technologies and displayed in the content area. This supports easy cross
// platform development for interfaces that do not require native polish.
// To support a new WebUI URL, return the appropriate web::WebUIIOSController in
// |CreateWebUIIOSControllerForURL|.
class WebViewWebUIIOSControllerFactory : public web::WebUIIOSControllerFactory {
 public:
  static WebViewWebUIIOSControllerFactory* GetInstance();

  WebViewWebUIIOSControllerFactory(const WebViewWebUIIOSControllerFactory&) =
      delete;
  WebViewWebUIIOSControllerFactory& operator=(
      const WebViewWebUIIOSControllerFactory&) = delete;

 protected:
  WebViewWebUIIOSControllerFactory();
  ~WebViewWebUIIOSControllerFactory() override;

  // web::WebUIIOSControllerFactory.
  NSInteger GetErrorCodeForWebUIURL(const GURL& url) const override;
  std::unique_ptr<web::WebUIIOSController> CreateWebUIIOSControllerForURL(
      web::WebUIIOS* web_ui,
      const GURL& url) const override;

 private:
  friend class base::NoDestructor<WebViewWebUIIOSControllerFactory>;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEBUI_WEB_VIEW_WEB_UI_IOS_CONTROLLER_FACTORY_H_
