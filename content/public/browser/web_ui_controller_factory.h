// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_FACTORY_H_

#include <memory>

#include "content/common/content_export.h"
#include "content/public/browser/web_ui.h"

class GURL;

namespace content {

class BrowserContext;
class WebUIController;

// Interface for an object which controls which URLs are considered WebUI URLs
// and creates WebUIController instances for given URLs.
class CONTENT_EXPORT WebUIControllerFactory {
 public:
  virtual ~WebUIControllerFactory() = default;

  // Call to register a factory.
  static void RegisterFactory(WebUIControllerFactory* factory);

  // Returns the number of registered factories.
  static int GetNumRegisteredFactoriesForTesting();

  // Returns a WebUIController instance for the given URL, or nullptr if the URL
  // doesn't correspond to a WebUI.
  virtual std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) = 0;

  // Gets the WebUI type for the given URL. This will return kNoWebUI if the
  // corresponding call to CreateWebUIForURL would fail, or something
  // non-nullptr if CreateWebUIForURL would succeed.
  virtual WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                                     const GURL& url) = 0;

  // Shorthand for the above, but returns a simple yes/no.
  // See also ContentClient::HasWebUIScheme, which only checks the scheme
  // (faster) and can be used to determine security policy.
  virtual bool UseWebUIForURL(BrowserContext* browser_context,
                              const GURL& url) = 0;

 private:
  friend class ScopedWebUIControllerFactoryRegistration;

  static void UnregisterFactoryForTesting(WebUIControllerFactory* factory);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_FACTORY_H_
