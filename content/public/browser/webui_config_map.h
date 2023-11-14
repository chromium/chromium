// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBUI_CONFIG_MAP_H_
#define CONTENT_PUBLIC_BROWSER_WEBUI_CONFIG_MAP_H_

#include <map>
#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "url/origin.h"

class GURL;

namespace content {

class BrowserContext;
class WebUIControllerFactory;
class WebUIConfig;

// Returned by GetWebUIConfigList()
struct CONTENT_EXPORT WebUIConfigInfo {
  url::Origin origin;
  bool enabled;
};

// Class that holds all WebUIConfigs for the browser.
//
// Embedders wishing to register WebUIConfigs should use
// AddWebUIConfig and AddUntrustedWebUIConfig.
//
// Underneath it uses a WebUIControllerFactory to hook into the rest of the
// WebUI infra.
class CONTENT_EXPORT WebUIConfigMap {
 public:
  static WebUIConfigMap& GetInstance();

  WebUIConfigMap();
  WebUIConfigMap(const WebUIConfigMap&) = delete;
  WebUIConfigMap& operator=(const WebUIConfigMap&) = delete;
  ~WebUIConfigMap();

  // Adds a chrome:// WebUIConfig. CHECKs if the WebUIConfig is for a
  // chrome-untrusted:// WebUIConfig.
  void AddWebUIConfig(std::unique_ptr<WebUIConfig> config);

  // Adds a chrome-untrusted:// WebUIConfig. CHECKs if the WebUIConfig is
  // for a chrome:// WebUIConfig.
  //
  // Although the scheme is included as part of the WebUIConfig, having
  // two separate methods for chrome:// and chrome-untrusted:// helps
  // readers tell what type of WebUIConfig is being added.
  void AddUntrustedWebUIConfig(std::unique_ptr<WebUIConfig> config);

  // Returns the WebUIConfig for |url| if it's registered and the WebUI is
  // enabled. (WebUIs can be disabled based on the profile or feature flags.)
  WebUIConfig* GetConfig(BrowserContext* browser_context, const GURL& url);

  // Removes and returns the WebUIConfig with |url|. Returns nullptr if
  // there is no WebUIConfig with |url|.
  std::unique_ptr<WebUIConfig> RemoveConfig(const GURL& url);

  // Gets a list of the origin (host + scheme) and enabled/disabled status of
  // all currently registered WebUIConfigs. If |browser_context| is null,
  // returns false for the enabled status for all UIs.
  std::vector<WebUIConfigInfo> GetWebUIConfigList(
      BrowserContext* browser_context);

 private:
  void AddWebUIConfigImpl(std::unique_ptr<WebUIConfig> config);

  using WebUIConfigMapImpl =
      std::map<url::Origin, std::unique_ptr<WebUIConfig>>;
  WebUIConfigMapImpl configs_map_;

  std::unique_ptr<WebUIControllerFactory> webui_controller_factory_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBUI_CONFIG_MAP_H_
