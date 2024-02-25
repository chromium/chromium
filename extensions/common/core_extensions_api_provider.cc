// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/core_extensions_api_provider.h"

#include <string_view>

#include "extensions/common/api/api_features.h"
#include "extensions/common/api/behavior_features.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/api/manifest_features.h"
#include "extensions/common/api/permission_features.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/permissions/extensions_api_permissions.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/grit/extensions_resources.h"

namespace extensions {

CoreExtensionsAPIProvider::CoreExtensionsAPIProvider() = default;
CoreExtensionsAPIProvider::~CoreExtensionsAPIProvider() = default;

void CoreExtensionsAPIProvider::AddAPIFeatures(FeatureProvider* provider) {
  AddCoreAPIFeatures(provider);
}

void CoreExtensionsAPIProvider::AddManifestFeatures(FeatureProvider* provider) {
  AddCoreManifestFeatures(provider);
}

void CoreExtensionsAPIProvider::AddPermissionFeatures(
    FeatureProvider* provider) {
  AddCorePermissionFeatures(provider);
}

void CoreExtensionsAPIProvider::AddBehaviorFeatures(FeatureProvider* provider) {
  AddCoreBehaviorFeatures(provider);
}

void CoreExtensionsAPIProvider::AddAPIJSONSources(
    JSONFeatureProviderSource* json_source) {
  json_source->LoadJSON(IDR_EXTENSION_API_FEATURES);
}

bool CoreExtensionsAPIProvider::IsAPISchemaGenerated(const std::string& name) {
  return api::GeneratedSchemas::IsGenerated(name);
}

std::string_view CoreExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  return api::GeneratedSchemas::Get(name);
}

void CoreExtensionsAPIProvider::RegisterPermissions(
    PermissionsInfo* permissions_info) {
  permissions_info->RegisterPermissions(
      api_permissions::GetPermissionInfos(),
      api_permissions::GetPermissionAliases());
}

void CoreExtensionsAPIProvider::RegisterManifestHandlers() {
  RegisterCommonManifestHandlers();
}

}  // namespace extensions
