// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_COMMON_SHELL_EXTENSIONS_API_PROVIDER_H_
#define EXTENSIONS_SHELL_COMMON_SHELL_EXTENSIONS_API_PROVIDER_H_

#include <string>
#include <string_view>

#include "extensions/common/extensions_api_provider.h"

namespace extensions {

class ShellExtensionsAPIProvider : public ExtensionsAPIProvider {
 public:
  ShellExtensionsAPIProvider();
  ShellExtensionsAPIProvider(const ShellExtensionsAPIProvider&) = delete;
  ShellExtensionsAPIProvider& operator=(const ShellExtensionsAPIProvider&) =
      delete;
  ~ShellExtensionsAPIProvider() override;

  // ExtensionsAPIProvider:
  void AddAPIFeatures(FeatureProvider* provider) override;
  void AddManifestFeatures(FeatureProvider* provider) override;
  void AddPermissionFeatures(FeatureProvider* provider) override;
  void AddBehaviorFeatures(FeatureProvider* provider) override;
  void AddAPIJSONSources(JSONFeatureProviderSource* json_source) override;
  bool IsAPISchemaGenerated(const std::string& name) override;
  std::string_view GetAPISchema(const std::string& name) override;
  void RegisterPermissions(PermissionsInfo* permissions_info) override;
  void RegisterManifestHandlers() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_COMMON_SHELL_EXTENSIONS_API_PROVIDER_H_
