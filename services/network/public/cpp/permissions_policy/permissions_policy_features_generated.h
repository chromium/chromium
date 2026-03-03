// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_GENERATED_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_GENERATED_H_

#include <string_view>

#include "services/network/public/cpp/permissions_policy/permissions_policy_features.h"

// Headers for the generated code from
// permissions_policy_features_generated.cc.tmpl.
namespace network {

PermissionsPolicyFeatureList GetBasePermissionsPolicyFeatureList();
void UpdatePermissionsPolicyFeatureListFlagDefaults(
    PermissionsPolicyFeatureList& mutable_feature_list);
PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureListUnloadNone();
PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureListUnloadAll();

// This means that `permissions_policy_features.json5` specifies
// `visibility: "IsolatedContext"` for this feature.
COMPONENT_EXPORT(NETWORK_CPP_WEB_PLATFORM)
bool IsPermissionsPolicyFeatureGuardedByIsolatedContext(
    std::string_view feature_name);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_GENERATED_H_
