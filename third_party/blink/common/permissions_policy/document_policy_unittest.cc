// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/document_policy.h"

#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/document_policy_features.h"
#include "third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom.h"

namespace blink {
namespace {

using DocumentPolicyTest = ::testing::Test;

// Helper function to convert literal to FeatureState.
template <class T>
DocumentPolicyFeatureState FeatureState(
    std::vector<std::pair<int32_t, T>> literal,
    const base::RepeatingCallback<PolicyValue(T)>& create_pv_cb) {
  DocumentPolicyFeatureState result;
  for (const auto& entry : literal) {
    result.insert({static_cast<mojom::DocumentPolicyFeature>(entry.first),
                   create_pv_cb.Run(entry.second)});
  }
  return result;
}

TEST_F(DocumentPolicyTest, MergeFeatureState) {
  base::RepeatingCallback<PolicyValue(bool)> bool_cb =
      base::BindRepeating(PolicyValue::CreateBool);
  base::RepeatingCallback<PolicyValue(double)> dec_double_cb =
      base::BindRepeating(PolicyValue::CreateDecDouble);
  base::RepeatingCallback<PolicyValue(int32_t)> enum_cb =
      base::BindRepeating(PolicyValue::CreateEnum);

  EXPECT_EQ(DocumentPolicy::MergeFeatureState(
                FeatureState<bool>(
                    {{1, false}, {2, false}, {3, true}, {4, true}, {5, false}},
                    bool_cb),
                FeatureState<bool>(
                    {{2, true}, {3, true}, {4, false}, {5, false}, {6, true}},
                    bool_cb)),
            FeatureState<bool>({{1, false},
                                {2, false},
                                {3, true},
                                {4, false},
                                {5, false},
                                {6, true}},
                               bool_cb));
  EXPECT_EQ(
      DocumentPolicy::MergeFeatureState(
          FeatureState<double>({{1, 1.0}, {2, 1.0}, {3, 1.0}, {4, 0.5}},
                               dec_double_cb),
          FeatureState<double>({{2, 0.5}, {3, 1.0}, {4, 1.0}, {5, 1.0}},
                               dec_double_cb)),
      FeatureState<double>({{1, 1.0}, {2, 0.5}, {3, 1.0}, {4, 0.5}, {5, 1.0}},
                           dec_double_cb));

  EXPECT_EQ(
      DocumentPolicy::MergeFeatureState(
          /* base_policy */ FeatureState<int32_t>(
              {{1, 1}, {2, 1}, {3, 1}, {4, 2}}, enum_cb),
          /* override_policy */ FeatureState<int32_t>(
              {{2, 2}, {3, 1}, {4, 1}, {5, 1}}, enum_cb)),
      FeatureState<int32_t>({{1, 1}, {2, 2}, {3, 1}, {4, 1}, {5, 1}}, enum_cb));
}

// IsPolicyCompatible should use default value for incoming policy when required
// policy specifies a value for a feature and incoming policy is missing value
// for that feature.
// TODO: This is not testable as only boolean features exist currently.
// TEST_F(DocumentPolicyTest, IsPolicyCompatible) {
//   mojom::DocumentPolicyFeature feature =
//       mojom::DocumentPolicyFeature::kLosslessImagesMaxBpp;
//   double default_policy_value =
//       GetDocumentPolicyFeatureInfoMap().at(feature).default_value.DoubleValue();
//   // Cap the default_policy_value, as it can be INF.
//   double strict_policy_value =
//       default_policy_value > 1.0 ? 1.0 : default_policy_value / 2;
//
//   EXPECT_FALSE(DocumentPolicy::IsPolicyCompatible(
//       DocumentPolicyFeatureState{
//           {feature, PolicyValue::CreateDecDouble(
//                         strict_policy_value)}}, /* required policy */
//       DocumentPolicyFeatureState{}              /* incoming policy */
//       ));
// }

// Verify that every Enum-typed Document Policy feature has a token mapping in
// document_policy_enum_values.h. If this test crashes with NOTREACHED(), a
// new Enum feature was added to document_policy_features.json5 without a
// corresponding case in DocumentPolicyEnumValueToToken.
TEST_F(DocumentPolicyTest, AllEnumFeaturesHaveTokenMappings) {
  for (const auto& [feature, info] : GetDocumentPolicyFeatureInfoMap()) {
    if (info.default_value.Type() != mojom::PolicyValueType::kEnum) {
      continue;
    }
    // Use value 1 (the first valid token, e.g. "eager") rather than the
    // default value 0 which intentionally has no token representation.
    DocumentPolicyFeatureState state{{feature, PolicyValue::CreateEnum(1)}};
    std::optional<std::string> serialized = DocumentPolicy::Serialize(state);
    EXPECT_TRUE(serialized.has_value())
        << "Enum feature " << static_cast<int>(feature)
        << " failed to serialize. Add a mapping in "
           "document_policy_enum_values.h.";
  }
}

// GetParsedPolicy() should return only the features whose value differs from
// the default (defaults are filtered out) and should preserve the endpoint
// map. This is the extraction used to inherit Document-Policy into
// local-scheme workers (about:, blob:, data:, filesystem:).
TEST_F(DocumentPolicyTest, GetParsedPolicyFiltersDefaultsAndKeepsEndpoints) {
  // Build a policy that sets js-profiling (default false) to true, with an
  // endpoint group, and explicitly sets force-load-at-top to its default.
  const auto kFeature = mojom::DocumentPolicyFeature::kJSProfiling;
  const auto kDefaultFeature = mojom::DocumentPolicyFeature::kForceLoadAtTop;
  DocumentPolicy::ParsedDocumentPolicy header_policy;
  header_policy.feature_state[kFeature] = PolicyValue::CreateBool(true);
  header_policy.endpoint_map[kFeature] = "endpoint-group";
  header_policy.feature_state[kDefaultFeature] = PolicyValue::CreateBool(false);
  header_policy.endpoint_map[kDefaultFeature] = "default-endpoint-group";

  std::unique_ptr<DocumentPolicy> policy =
      DocumentPolicy::CreateWithHeaderPolicy(header_policy);

  DocumentPolicy::ParsedDocumentPolicy parsed = policy->GetParsedPolicy();

  // Only the non-default feature is present; all defaulted features are
  // filtered out.
  EXPECT_EQ(parsed.feature_state.size(), 1u);
  ASSERT_TRUE(parsed.feature_state.contains(kFeature));
  EXPECT_EQ(parsed.feature_state.at(kFeature), PolicyValue::CreateBool(true));
  EXPECT_FALSE(parsed.feature_state.contains(kDefaultFeature));

  // Endpoint mappings are retained only for non-default features.
  EXPECT_EQ(parsed.endpoint_map.size(), 1u);
  EXPECT_EQ(parsed.endpoint_map.at(kFeature), "endpoint-group");
  EXPECT_FALSE(parsed.endpoint_map.contains(kDefaultFeature));
}

}  // namespace
}  // namespace blink
