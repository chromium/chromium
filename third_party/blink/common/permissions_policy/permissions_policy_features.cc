// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"

#include "base/no_destructor.h"
#include "third_party/blink/common/permissions_policy/permissions_policy_features_generated.h"

// This file contains static code that is combined with templated code of
// permissions_policy_features.cc.tmpl.

namespace blink {

const PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureList() {
  static const base::NoDestructor<PermissionsPolicyFeatureList> feature_list(
      [] {
        PermissionsPolicyFeatureList map =
            GetBasePermissionsPolicyFeatureList();

        UpdatePermissionsPolicyFeatureListFlagDefaults(map);
        return map;
      }());
  return *feature_list;
}

void UpdatePermissionsPolicyFeatureListForTesting() {
  const PermissionsPolicyFeatureList& feature_list =
      GetPermissionsPolicyFeatureList();
  PermissionsPolicyFeatureList& mutable_feature_list =
      const_cast<PermissionsPolicyFeatureList&>(feature_list);
  UpdatePermissionsPolicyFeatureListFlagDefaults(mutable_feature_list);
}

}  // namespace blink
