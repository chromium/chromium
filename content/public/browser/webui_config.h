// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBUI_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_WEBUI_CONFIG_H_

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;
class WebUIController;
class WebUI;

// Class that stores properties for a WebUI.
//
// Clients that implement WebUI pages subclass WebUIConfig, overload the
// relevant methods and add an instance of their subclass to WebUIConfigMap.
//
// WebUIConfig is used when navigating to chrome:// or chrome-untrusted://
// pages to create the WebUIController and register the URLDataSource for
// the WebUI.
//
// WebUI pages are currently being migrated to use WebUIConfig so not
// all existing WebUI pages use this.
class CONTENT_EXPORT WebUIConfig {
 public:
  explicit WebUIConfig(std::string_view scheme, std::string_view host);
  virtual ~WebUIConfig();
  WebUIConfig(const WebUIConfig&) = delete;
  WebUIConfig& operator=(const WebUIConfig&) = delete;

  // Scheme for the WebUI.
  const std::string& scheme() const { return scheme_; }

  // Host the WebUI serves.
  const std::string& host() const { return host_; }

  // Returns whether the WebUI is enabled e.g. the necessary feature flags are
  // on/off, the WebUI is enabled in incognito, etc. Defaults to true.
  virtual bool IsWebUIEnabled(BrowserContext* browser_context);

  // Returns whether the WebUI should be used for the given |url|. Not all
  // WebUIs should be created for all requests to their host. Defaults to true.
  virtual bool ShouldHandleURL(const GURL& url);

  // Returns a WebUIController for WebUI and GURL.
  //
  // URLDataSource is usually created in the constructor of WebUIController. The
  // URLDataSource should be the same as the one registered in
  // `RegisterURLDataSource()` or resources will fail to load.
  virtual std::unique_ptr<WebUIController> CreateWebUIController(
      WebUI* web_ui,
      const GURL& url) = 0;

  // This is called when registering or updating a Service Worker.
  //
  // The URLDataSource here should be the same as the one registered in
  // the WebUIController or resources will fail to load.
  virtual void RegisterURLDataSource(BrowserContext* browser_context) {}

 private:
  const std::string scheme_;
  const std::string host_;
};

// Templated class with an implementation for CreateWebUIController. Prefer
// to use this over WebUIConfig if the WebUIController can be created with
// a single WebUI argument.
template <typename T>
class CONTENT_EXPORT DefaultWebUIConfig : public WebUIConfig {
 public:
  explicit DefaultWebUIConfig(std::string_view scheme, std::string_view host)
      : WebUIConfig(scheme, host) {}
  ~DefaultWebUIConfig() override = default;

  std::unique_ptr<WebUIController> CreateWebUIController(
      WebUI* web_ui,
      const GURL& url) override {
    // Disallow dual constructibility.
    // The controller can be constructed either by T(WebUI*) or
    // T(WebUI*, const GURL&), ...
    static_assert(std::is_constructible_v<T, WebUI*> ||
                  std::is_constructible_v<T, WebUI*, const GURL&>);
    // ..., but not both.
    static_assert(!(std::is_constructible_v<T, WebUI*> &&
                    std::is_constructible_v<T, WebUI*, const GURL&>));
    if constexpr (std::is_constructible_v<T, WebUI*>) {
      return std::make_unique<T>(web_ui);
    }
    if constexpr (std::is_constructible_v<T, WebUI*, const GURL&>) {
      return std::make_unique<T>(web_ui, url);
    }
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBUI_CONFIG_H_
