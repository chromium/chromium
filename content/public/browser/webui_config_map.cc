// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webui_config_map.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace content {

namespace {

// Owned by WebUIConfigMap. Used to hook up with the existing WebUI infra.
class WebUIConfigMapWebUIControllerFactory : public WebUIControllerFactory {
 public:
  explicit WebUIConfigMapWebUIControllerFactory(WebUIConfigMap& config_map)
      : config_map_(config_map) {}
  ~WebUIConfigMapWebUIControllerFactory() override = default;

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    auto* config = config_map_->GetConfig(browser_context, url);
    return config ? reinterpret_cast<WebUI::TypeID>(config) : WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return config_map_->GetConfig(browser_context, url);
  }

  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
    auto* config = config_map_->GetConfig(browser_context, url);
    return config ? config->CreateWebUIController(web_ui, url) : nullptr;
  }

 private:
  // Keeping a reference should be safe since this class is owned by
  // WebUIConfigMap.
  const raw_ref<WebUIConfigMap> config_map_;
};

}  // namespace

// static
WebUIConfigMap& WebUIConfigMap::GetInstance() {
  static base::NoDestructor<WebUIConfigMap> instance;
  return *instance.get();
}

WebUIConfigMap::WebUIConfigMap()
    : webui_controller_factory_(
          std::make_unique<WebUIConfigMapWebUIControllerFactory>(*this)) {
  WebUIControllerFactory::RegisterFactory(webui_controller_factory_.get());
}

WebUIConfigMap::~WebUIConfigMap() = default;

void WebUIConfigMap::AddWebUIConfig(std::unique_ptr<WebUIConfig> config) {
  CHECK_EQ(config->scheme(), kChromeUIScheme);
  AddWebUIConfigImpl(std::move(config));
}

void WebUIConfigMap::AddUntrustedWebUIConfig(
    std::unique_ptr<WebUIConfig> config) {
  CHECK_EQ(config->scheme(), kChromeUIUntrustedScheme);
  AddWebUIConfigImpl(std::move(config));
}

void WebUIConfigMap::AddWebUIConfigImpl(std::unique_ptr<WebUIConfig> config) {
  GURL url(base::StrCat(
      {config->scheme(), url::kStandardSchemeSeparator, config->host()}));
  auto it = configs_map_.emplace(url::Origin::Create(url), std::move(config));
  // CHECK if a WebUIConfig with the same host was already added.
  CHECK(it.second) << url;
}

WebUIConfig* WebUIConfigMap::GetConfig(BrowserContext* browser_context,
                                       const GURL& url) {
  // "filesystem:" and "blob:" get dropped by url::Origin::Create() below. We
  // don't want navigations to these URLs to have WebUI bindings, e.g.
  // chrome.send() or Mojo.bindInterface(), since some WebUIs currently expose
  // untrusted content via these schemes.
  if (url.scheme() != kChromeUIScheme &&
      url.scheme() != kChromeUIUntrustedScheme) {
    return nullptr;
  }

  auto origin_and_config = configs_map_.find(url::Origin::Create(url));
  if (origin_and_config == configs_map_.end()) {
    return nullptr;
  }

  auto& config = origin_and_config->second;
  if (!config->IsWebUIEnabled(browser_context) ||
      !config->ShouldHandleURL(url)) {
    return nullptr;
  }

  return config.get();
}

std::unique_ptr<WebUIConfig> WebUIConfigMap::RemoveConfig(const GURL& url) {
  CHECK(url.scheme() == kChromeUIScheme ||
        url.scheme() == kChromeUIUntrustedScheme);

  auto it = configs_map_.find(url::Origin::Create(url));
  if (it == configs_map_.end()) {
    return nullptr;
  }

  auto webui_config = std::move(it->second);
  configs_map_.erase(it);
  return webui_config;
}

std::vector<WebUIConfigInfo> WebUIConfigMap::GetWebUIConfigList(
    BrowserContext* browser_context) {
  std::vector<WebUIConfigInfo> origins;
  origins.reserve(configs_map_.size());
  for (auto& it : configs_map_) {
    auto& webui_config = it.second;
    origins.push_back({
        .origin = it.first,
        .enabled =
            browser_context && webui_config->IsWebUIEnabled(browser_context),
    });
  }
  return origins;
}

}  // namespace content
