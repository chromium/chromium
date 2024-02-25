// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CHROME_WEB_UI_IOS_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CHROME_WEB_UI_IOS_CONTROLLER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"

class GURL;

class ChromeWebUIIOSControllerFactory : public web::WebUIIOSControllerFactory {
 public:
  std::unique_ptr<web::WebUIIOSController> CreateWebUIIOSControllerForURL(
      web::WebUIIOS* web_ui,
      const GURL& url) const override;

  NSInteger GetErrorCodeForWebUIURL(const GURL& url) const override;

  static ChromeWebUIIOSControllerFactory* GetInstance();

  ChromeWebUIIOSControllerFactory(const ChromeWebUIIOSControllerFactory&) =
      delete;
  ChromeWebUIIOSControllerFactory& operator=(
      const ChromeWebUIIOSControllerFactory&) = delete;

 protected:
  ChromeWebUIIOSControllerFactory();
  ~ChromeWebUIIOSControllerFactory() override;

 private:
  friend class base::NoDestructor<ChromeWebUIIOSControllerFactory>;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CHROME_WEB_UI_IOS_CONTROLLER_FACTORY_H_
