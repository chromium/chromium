// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_GENERATED_H_
#define THIRD_PARTY_BLINK_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_GENERATED_H_

#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"

// Headers for the generated code from permissions_policy_features.cc.tmpl.
namespace blink {

PermissionsPolicyFeatureList GetBasePermissionsPolicyFeatureList();
void UpdatePermissionsPolicyFeatureListFlagDefaults(
    PermissionsPolicyFeatureList& mutable_feature_list);
PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureListUnloadNone();
PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureListUnloadAll();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_GENERATED_H_
