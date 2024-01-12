// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/shell_extensions_api_provider.h"

#include <string_view>

#include "extensions/shell/common/api/shell_api_features.h"
#include "extensions/shell/grit/app_shell_resources.h"

namespace extensions {

ShellExtensionsAPIProvider::ShellExtensionsAPIProvider() = default;
ShellExtensionsAPIProvider::~ShellExtensionsAPIProvider() = default;

void ShellExtensionsAPIProvider::AddAPIFeatures(FeatureProvider* provider) {
  AddShellAPIFeatures(provider);
}

void ShellExtensionsAPIProvider::AddManifestFeatures(
    FeatureProvider* provider) {
  // No shell-specific manifest features.
}

void ShellExtensionsAPIProvider::AddPermissionFeatures(
    FeatureProvider* provider) {
  // No shell-specific permission features.
}

void ShellExtensionsAPIProvider::AddBehaviorFeatures(
    FeatureProvider* provider) {
  // No shell-specific behavior features.
}

void ShellExtensionsAPIProvider::AddAPIJSONSources(
    JSONFeatureProviderSource* json_source) {
  // No shell-specific APIs.
}

bool ShellExtensionsAPIProvider::IsAPISchemaGenerated(const std::string& name) {
  return false;
}

std::string_view ShellExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  return std::string_view();
}

void ShellExtensionsAPIProvider::RegisterPermissions(
    PermissionsInfo* permissions_info) {}

void ShellExtensionsAPIProvider::RegisterManifestHandlers() {}

}  // namespace extensions
