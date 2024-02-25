// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSIONS_API_PROVIDER_H_
#define EXTENSIONS_COMMON_EXTENSIONS_API_PROVIDER_H_

#include <string>
#include <string_view>

namespace extensions {
class FeatureProvider;
class JSONFeatureProviderSource;
class PermissionsInfo;

// A class to provide API-specific bits and bobs to the extensions system.
// This allows for composition of multiple providers, so that we can easily
// selectively add features in different configurations.
class ExtensionsAPIProvider {
 public:
  ExtensionsAPIProvider() = default;
  ExtensionsAPIProvider(const ExtensionsAPIProvider&) = delete;
  ExtensionsAPIProvider& operator=(const ExtensionsAPIProvider&) = delete;
  virtual ~ExtensionsAPIProvider() = default;

  // Adds feature definitions to the given |provider| of the specified type.
  virtual void AddAPIFeatures(FeatureProvider* provider) = 0;
  virtual void AddManifestFeatures(FeatureProvider* provider) = 0;
  virtual void AddPermissionFeatures(FeatureProvider* provider) = 0;
  virtual void AddBehaviorFeatures(FeatureProvider* provider) = 0;

  // Adds resources containing the JSON API definitions.
  virtual void AddAPIJSONSources(JSONFeatureProviderSource* json_source) = 0;

  // Returns true if this provider knows about a generated schema for the given
  // api |name|.
  virtual bool IsAPISchemaGenerated(const std::string& name) = 0;

  // Returns a the contents of the generated schema for the given api |name|,
  // or an empty string if this provider doesn't know of the generated API.
  virtual std::string_view GetAPISchema(const std::string& name) = 0;

  // Registers permissions for any associated API features.
  virtual void RegisterPermissions(PermissionsInfo* permissions_info) = 0;

  // Registers manifest handlers for any associated API features.
  virtual void RegisterManifestHandlers() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSIONS_API_PROVIDER_H_
