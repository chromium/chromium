// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_GENERATED_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_GENERATED_H_

#include "services/network/public/cpp/permissions_policy/permissions_policy_features.h"

// Headers for the generated code from
// permissions_policy_features_generated.cc.tmpl.
namespace network {

PermissionsPolicyFeatureList GetBasePermissionsPolicyFeatureList();
void UpdatePermissionsPolicyFeatureListFlagDefaults(
    PermissionsPolicyFeatureList& mutable_feature_list);
PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureListUnloadNone();
PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureListUnloadAll();

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_GENERATED_H_
