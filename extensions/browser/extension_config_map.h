// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_CONFIG_MAP_H_
#define EXTENSIONS_BROWSER_EXTENSION_CONFIG_MAP_H_

#include <memory>
#include <optional>
#include <string>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/template_expressions.h"

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

  // Returns true if JS error reporting is enabled for this extension.
  virtual bool IsJsErrorReportingEnabled() const;

  // Returns true if JS errors in this extension should crash the browser in
  // development builds for early detection in test/local environments.
  virtual bool ShouldCrashOnJsErrorInDevelopmentBuild() const;

 private:
  const ExtensionId extension_id_;
  std::optional<ui::TemplateReplacements> template_replacements_;
};

// A registry for component extension configuration providers. It decouples the
// core extensions layer and observers from individual feature details by
// allowing features to register configuration providers
// (`ExtensionConfigProvider`).
class ExtensionConfigMap : public KeyedService {
 public:
  ExtensionConfigMap();
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
  // nullptr if no provider is registered.
  ExtensionConfigProvider* GetConfigProvider(const ExtensionId& extension_id);

  void ClearProvidersForTesting();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  absl::flat_hash_map<ExtensionId, std::unique_ptr<ExtensionConfigProvider>>
      providers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_CONFIG_MAP_H_
