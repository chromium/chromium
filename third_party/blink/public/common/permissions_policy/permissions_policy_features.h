// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_H_

#include <map>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace blink {

// The PermissionsPolicyFeatureDefault enum defines the default enable state for
// a feature. For a top-level frame, this is the default enable state; for an
// iframe, this is the default enable state unless the iframe has an 'allow'
// attribute.
//
// See |PermissionsPolicy::InheritedValueForFeature| for usage.
//
// The 2 possibilities map directly to Permissions Policy Allowlist semantics.
//
// The default values for each feature are set in
// GetPermissionsPolicyFeatureList.
enum class PermissionsPolicyFeatureDefault {
  // Equivalent to ["self"]. If this default policy is in effect for a frame,
  // then the feature will be enabled for that frame, and any same-origin
  // child frames, but not for any cross-origin child frames.
  EnableForSelf,

  // Equivalent to ["*"]. If in effect for a frame, then the feature is
  // enabled for that frame and all of its children.
  EnableForAll
};

using PermissionsPolicyFeatureList =
    std::map<mojom::PermissionsPolicyFeature, PermissionsPolicyFeatureDefault>;

BLINK_COMMON_EXPORT const PermissionsPolicyFeatureList&
GetPermissionsPolicyFeatureList();

// Updates the PermissionPolicyFeatureList based on the current feature flags.
// For efficiency, `GetPermissionPolicyFeatureList()` only calculates the
// default permissions policy once, so it does not track changes in feature
// flags that occur between tests. This function is intended to be used in tests
// that depend on the permission policy being set based the value on a feature
// flag to avoid flakiness. Note that, like the general feature flag
// calculation, if the flags for multiple `default_value_behind_flag` are
// enabled, the default from the first listed is used.
BLINK_COMMON_EXPORT void UpdatePermissionsPolicyFeatureListForTesting();

// TODO(iclelland): Generate, instead of this map, a set of bool flags, one
// for each feature, as all features are supposed to be represented here.
using PermissionsPolicyFeatureState =
    std::map<mojom::PermissionsPolicyFeature, bool>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_H_
