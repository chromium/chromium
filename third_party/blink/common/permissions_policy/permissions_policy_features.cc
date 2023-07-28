// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"

#include "third_party/blink/common/permissions_policy/permissions_policy_features_generated.h"
#include "third_party/blink/public/common/features.h"

// This file contains static code that is combined with templated code of
// permissions_policy_features.cc.tmpl.

namespace blink {

const PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureList() {
  if (base::FeatureList::IsEnabled(features::kDeprecateUnload)) {
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
