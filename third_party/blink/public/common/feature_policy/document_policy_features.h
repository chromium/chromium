// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_DOCUMENT_POLICY_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_DOCUMENT_POLICY_FEATURES_H_

#include "base/containers/flat_map.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/feature_policy/policy_value.h"
#include "third_party/blink/public/mojom/feature_policy/document_policy_feature.mojom.h"

namespace blink {

struct DocumentPolicyFeatureInfo {
  std::string feature_name;
  PolicyValue default_value;
};

using DocumentPolicyFeatureInfoMap =
    base::flat_map<mojom::DocumentPolicyFeature, DocumentPolicyFeatureInfo>;

using DocumentPolicyFeatureState =
    base::flat_map<mojom::DocumentPolicyFeature, PolicyValue>;

using DocumentPolicyNameFeatureMap =
    base::flat_map<std::string, mojom::DocumentPolicyFeature>;

BLINK_COMMON_EXPORT const DocumentPolicyFeatureInfoMap&
GetDocumentPolicyFeatureInfoMap();

BLINK_COMMON_EXPORT const DocumentPolicyNameFeatureMap&
GetDocumentPolicyNameFeatureMap();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_DOCUMENT_POLICY_FEATURES_H_
