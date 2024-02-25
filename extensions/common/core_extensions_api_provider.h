// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CORE_EXTENSIONS_API_PROVIDER_H_
#define EXTENSIONS_COMMON_CORE_EXTENSIONS_API_PROVIDER_H_

#include <string_view>

#include "extensions/common/extensions_api_provider.h"

namespace extensions {

class CoreExtensionsAPIProvider : public ExtensionsAPIProvider {
 public:
  CoreExtensionsAPIProvider();
  CoreExtensionsAPIProvider(const CoreExtensionsAPIProvider&) = delete;
  CoreExtensionsAPIProvider& operator=(const CoreExtensionsAPIProvider&) =
      delete;
  ~CoreExtensionsAPIProvider() override;

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

#endif  // EXTENSIONS_COMMON_CORE_EXTENSIONS_API_PROVIDER_H_
