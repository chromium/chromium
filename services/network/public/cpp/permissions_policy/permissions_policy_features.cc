// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/permissions_policy_features.h"

#include "base/command_line.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features_generated.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features_internal.h"
#include "url/origin.h"

// This file contains static code that is combined with templated code of
// permissions_policy_features_generated.cc.tmpl.

namespace network {

const PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureList(
    const url::Origin& origin) {
  // Respect enterprise policy.
  if (!base::CommandLine::InitializedForCurrentProcess() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          network::switches::kForcePermissionPolicyUnloadDefaultEnabled)) {
    return GetPermissionsPolicyFeatureListUnloadAll();
  }

  // Consider the finch flags and params.
  if (base::FeatureList::IsEnabled(network::features::kDeprecateUnload) &&
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

}  // namespace network
