// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_CONFIG_MAP_H_
#define EXTENSIONS_BROWSER_EXTENSION_CONFIG_MAP_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/template_expressions.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

// An interface for features to supply configuration and telemetry settings for
// a component extension. Features should subclass this provider and transfer
// ownership of the instance to the `ExtensionConfigMap` KeyedService for a
// given BrowserContext.
class ExtensionConfigProvider {
 public:
  explicit ExtensionConfigProvider(ExtensionId extension_id);
  virtual ~ExtensionConfigProvider();

  // Returns the ID of the component extension configured by this provider.
  const ExtensionId& extension_id() const { return extension_id_; }

  // Returns the `$i18n{key}` template replacements for this component
  // extension.
  const ui::TemplateReplacements* GetTemplateReplacements(
      content::BrowserContext& context);

  // Returns dictionary data for this component extension to supply `$i18n{key}`
  // template replacements and `loadTimeData` in dynamic ES modules (e.g.,
  // `strings.m.js`).
  virtual base::DictValue GetLoadTimeData(content::BrowserContext& context);

  // Returns true if `path` is a dynamically generated resource (e.g.
  // `/strings.m.js`) supplied by this config provider.
  bool IsDynamicResource(const std::string& path) const;

  // Generates the JavaScript content for the dynamic resource at `path`.
  std::string GetDynamicResourceContent(const std::string& path,
                                        content::BrowserContext& context);

  // Returns a custom chrome:// host string (e.g. "aim") if this component
  // extension opts in to handling chrome://<host> navigations.
  virtual std::string_view GetChromeURLHost() const;

  // Sets the default extension resource path to load when navigating to the
  // root chrome://<host>/ URL (e.g., "/aim_eligibility.html"). `resource_path`
  // must start with "/".
  void SetDefaultResource(std::string_view resource_path);

  // Maps a chrome:// `url_path` to an extension `resource_path` (e.g.,
  // AddResourcePath("/eligibility", "/aim_eligibility.html")). Both `url_path`
  // and `resource_path` must start with "/".
  void AddResourcePath(std::string_view url_path,
                       std::string_view resource_path);

  // Returns the extension resource path mapped to `url_path`, falling back to
  // `url_path` itself if no mapping is registered.
  std::string_view GetResourcePathForUrlPath(std::string_view url_path) const;

  // Returns the user-visible chrome:// URL path mapped to `resource_path`
  // (e.g., "/" for the default resource, or "/eligibility" for a mapped URL
  // path), falling back to `resource_path` itself if no mapping is registered.
  std::string_view GetUrlPathForResourcePath(
      std::string_view resource_path) const;

  // Returns true if JS error reporting is enabled for this extension.
  virtual bool IsJsErrorReportingEnabled() const;

  // Returns true if JS errors in this extension should crash the browser in
  // development builds for early detection in test/local environments.
  virtual bool ShouldCrashOnJsErrorInDevelopmentBuild() const;

  // Returns true if this extension is allowed to use the Unbounded Element API.
  // Defaults to false.
  virtual bool IsUnboundedElementAllowed() const;

 private:
  const ExtensionId extension_id_;
  std::optional<ui::TemplateReplacements> template_replacements_;
  base::flat_map<std::string, std::string> path_map_;
  base::flat_map<std::string, std::string> reverse_path_map_;
};

// A registry for component extension configuration providers. It decouples the
// core extensions layer and observers from individual feature details by
// allowing features to register configuration providers
// (`ExtensionConfigProvider`).
class ExtensionConfigMap : public KeyedService {
 public:
  // Forward and reverse `content::BrowserURLHandler` pairs.
  // `HandleChromeURL` rewrites chrome://<host>/<url_path> navigations to the
  // corresponding chrome-extension://<extension_id>/<resource_path> URL.
  // `HandleChromeURLReverse` converts
  // chrome-extension://<extension_id>/<resource_path> URLs back to
  // user-visible chrome://<host>/<url_path> URLs.
  static bool HandleChromeURL(GURL* url, content::BrowserContext* context);
  static bool HandleChromeURLReverse(GURL* url,
                                     content::BrowserContext* context);

  explicit ExtensionConfigMap(content::BrowserContext& browser_context);
  ExtensionConfigMap(const ExtensionConfigMap&) = delete;
  ExtensionConfigMap& operator=(const ExtensionConfigMap&) = delete;
  ~ExtensionConfigMap() override;

  // Registers a provider for component extension configuration, taking
  // ownership of the provider instance.
  void RegisterConfigProvider(
      std::unique_ptr<ExtensionConfigProvider> provider);

  // Returns the ExtensionConfigProvider registered for `extension`, or nullptr
  // if no provider is registered or if `extension` is not a component
  // extension.
  ExtensionConfigProvider* GetConfigProvider(const Extension& extension);

  // Returns the ExtensionConfigProvider registered for `extension_id`, or
  // nullptr if no provider is registered or if the extension is not enabled in
  // `ExtensionRegistry` as a component extension.
  ExtensionConfigProvider* GetConfigProviderByExtensionId(
      std::string_view extension_id);

  // Returns the ExtensionConfigProvider registered for `chrome_url_host`, or
  // nullptr if no provider is registered for that host or if the corresponding
  // extension is not enabled in `ExtensionRegistry` as a component extension.
  ExtensionConfigProvider* GetConfigProviderByChromeURLHost(
      std::string_view chrome_url_host);

  // Returns true if the extension with `extension_id` is allowed to use the
  // Unbounded Element API.
  bool IsUnboundedElementAllowed(const ExtensionId& extension_id);

  void ClearProvidersForTesting();

 private:
  const raw_ref<content::BrowserContext> browser_context_;
  absl::flat_hash_map<ExtensionId, std::unique_ptr<ExtensionConfigProvider>>
      providers_map_;
  absl::flat_hash_map<std::string, ExtensionId> chrome_url_host_map_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_CONFIG_MAP_H_
