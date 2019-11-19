// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/document_policy.h"

#include "base/no_destructor.h"

namespace blink {

// static
std::unique_ptr<DocumentPolicy> DocumentPolicy::CreateWithRequiredPolicy(
    const FeatureState& required_policy) {
  return CreateWithRequiredPolicy(required_policy, GetFeatureDefaults());
}

bool DocumentPolicy::IsFeatureEnabled(
    mojom::FeaturePolicyFeature feature) const {
  mojom::PolicyValueType feature_type = GetFeatureDefaults().at(feature).Type();
  // TODO(iclelland): Generate this switch block
  switch (feature) {
    case mojom::FeaturePolicyFeature::kFontDisplay:
      return font_display_;
    case mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages:
      return PolicyValue(unoptimized_lossless_images_) >=
             PolicyValue::CreateMaxPolicyValue(feature_type);
    default:
      NOTREACHED();
      return true;
  }
}

bool DocumentPolicy::IsFeatureEnabled(
    mojom::FeaturePolicyFeature feature,
    const PolicyValue& threshold_value) const {
  return GetFeatureValue(feature) >= threshold_value;
}

PolicyValue DocumentPolicy::GetFeatureValue(
    mojom::FeaturePolicyFeature feature) const {
  // TODO(iclelland): Generate this switch block
  switch (feature) {
    case mojom::FeaturePolicyFeature::kFontDisplay:
      return PolicyValue(font_display_);
    case mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages:
      return PolicyValue(unoptimized_lossless_images_);
    default:
      NOTREACHED();
      return PolicyValue(false);
  }
}

bool DocumentPolicy::IsFeatureSupported(
    mojom::FeaturePolicyFeature feature) const {
  // TODO(iclelland): Generate this switch block
  switch (feature) {
    case mojom::FeaturePolicyFeature::kFontDisplay:
    case mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages:
      return true;
    default:
      return false;
  }
}

void DocumentPolicy::SetHeaderPolicy(
    const ParsedDocumentPolicy& parsed_header) {
  for (const ParsedDocumentPolicyDeclaration& parsed_declaration :
       parsed_header) {
    mojom::FeaturePolicyFeature feature = parsed_declaration.feature;
    // TODO(iclelland): Generate this switch block
    switch (feature) {
      case mojom::FeaturePolicyFeature::kFontDisplay:
        font_display_ = parsed_declaration.value.BoolValue();
        break;
      case mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages:
        unoptimized_lossless_images_ = parsed_declaration.value.DoubleValue();
        break;
      default:
        NOTREACHED();
    }
  }
}

void DocumentPolicy::UpdateFeatureState(const FeatureState& feature_state) {
  for (const auto& feature_and_value : feature_state) {
    // TODO(iclelland): Generate this switch block
    switch (feature_and_value.first) {
      case mojom::FeaturePolicyFeature::kFontDisplay:
        font_display_ = feature_and_value.second.BoolValue();
        break;
      case mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages:
        unoptimized_lossless_images_ = feature_and_value.second.DoubleValue();
        break;
      default:
        NOTREACHED();
    }
  }
}

DocumentPolicy::FeatureState DocumentPolicy::GetFeatureState() const {
  FeatureState feature_state;
  // TODO(iclelland): Generate this block
  feature_state[mojom::FeaturePolicyFeature::kFontDisplay] =
      PolicyValue(font_display_);
  feature_state[mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages] =
      PolicyValue(unoptimized_lossless_images_);
  return feature_state;
}

DocumentPolicy::DocumentPolicy(const FeatureState& defaults) {
  UpdateFeatureState(defaults);
}

DocumentPolicy::~DocumentPolicy() = default;

// static
std::unique_ptr<DocumentPolicy> DocumentPolicy::CreateWithRequiredPolicy(
    const FeatureState& required_policy,
    const DocumentPolicy::FeatureState& defaults) {
  std::unique_ptr<DocumentPolicy> new_policy =
      base::WrapUnique(new DocumentPolicy(defaults));
  new_policy->UpdateFeatureState(required_policy);
  return new_policy;
}

// static
// TODO(iclelland): This list just contains two sample features for use during
// development. It should be generated from definitions in a feature
// configuration json5 file.
const DocumentPolicy::FeatureState& DocumentPolicy::GetFeatureDefaults() {
  static base::NoDestructor<FeatureState> default_feature_list(
      {{mojom::FeaturePolicyFeature::kFontDisplay, PolicyValue(false)},
       {mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
        PolicyValue(2.0f)}});
  return *default_feature_list;
}

bool DocumentPolicy::IsPolicyCompatible(
    const ParsedDocumentPolicy& required_policy) {
  FeatureState p_map = ParsedDocumentPolicyToFeatureState(RequiredPolicy());
  for (const ParsedDocumentPolicyDeclaration& req_p : required_policy) {
    // feature value > threshold => enabled
    // value_a > value_b => value_a looser than value_b
    if (p_map[req_p.feature] > req_p.value)
      return false;
  }
  return true;
}

// static
DocumentPolicy::FeatureState DocumentPolicy::ParsedDocumentPolicyToFeatureState(
    const ParsedDocumentPolicy& policies) {
  FeatureState result;
  for (const ParsedDocumentPolicyDeclaration& policy : policies)
    result[policy.feature] = policy.value;
  return result;
}

ParsedDocumentPolicy DocumentPolicy::RequiredPolicy() const {
  // TODO(iclelland): This is currently a placeholder.
  // To be implemented later.
  return ParsedDocumentPolicy();
}

}  // namespace blink
