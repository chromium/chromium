// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"

#include "base/command_line.h"
#include "third_party/blink/common/permissions_policy/permissions_policy_features_generated.h"
#include "third_party/blink/common/permissions_policy/permissions_policy_features_internal.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "url/origin.h"

// This file contains static code that is combined with templated code of
// permissions_policy_features_generated.cc.tmpl.

namespace blink {

const PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureList(
    const url::Origin& origin) {
  // Respect enterprise policy.
  if (!base::CommandLine::InitializedForCurrentProcess() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForcePermissionPolicyUnloadDefaultEnabled)) {
    return GetPermissionsPolicyFeatureListUnloadAll();
  }

  // Consider the finch flags and params.
  if (base::FeatureList::IsEnabled(features::kDeprecateUnload) &&
      UnloadDeprecationAllowedForOrigin(origin)) {
    return GetPermissionsPolicyFeatureListUnloadNone();
  }
  return GetPermissionsPolicyFeatureListUnloadAll();
}

void UpdatePermissionsPolicyFeatureListForTesting() {
  UpdatePermissionsPolicyFeatureListFlagDefaults(
      GetPermissionsPolicyFeatureListUnloadAll());
  UpdatePermissionsPolicyFeatureListFlagDefaults(
      GetPermissionsPolicyFeatureListUnloadNone());
}

}  // namespace blink
