// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/document_policy.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "net/http/structured_headers.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace blink {

// static
std::unique_ptr<DocumentPolicy> DocumentPolicy::CreateWithHeaderPolicy(
    const ParsedDocumentPolicy& header_policy) {
  DocumentPolicyFeatureState feature_defaults;
  for (const auto& entry : GetDocumentPolicyFeatureInfoMap())
    feature_defaults.emplace(entry.first, entry.second.default_value);
  return CreateWithHeaderPolicy(header_policy.feature_state,
                                header_policy.endpoint_map, feature_defaults);
}

// static
std::unique_ptr<DocumentPolicy> DocumentPolicy::CopyStateFrom(
    const DocumentPolicy* source) {
  if (!source)
    return nullptr;

  std::unique_ptr<DocumentPolicy> new_policy =
      DocumentPolicy::CreateWithHeaderPolicy(
          {/* header_policy */ {}, /* endpoint_map */ {}});

  new_policy->internal_feature_state_ = source->internal_feature_state_;
  new_policy->endpoint_map_ = source->endpoint_map_;
  return new_policy;
}

namespace {
net::structured_headers::Item PolicyValueToItem(const PolicyValue& value) {
  switch (value.Type()) {
    case mojom::PolicyValueType::kBool:
      return net::structured_headers::Item{value.BoolValue()};
    case mojom::PolicyValueType::kDecDouble:
      return net::structured_headers::Item{value.DoubleValue()};
    default:
      NOTREACHED_IN_MIGRATION();
      return net::structured_headers::Item{
          nullptr, net::structured_headers::Item::ItemType::kNullType};
  }
}

}  // namespace

// static
std::optional<std::string> DocumentPolicy::Serialize(
    const DocumentPolicyFeatureState& policy) {
  return DocumentPolicy::SerializeInternal(policy,
                                           GetDocumentPolicyFeatureInfoMap());
}

// static
std::optional<std::string> DocumentPolicy::SerializeInternal(
    const DocumentPolicyFeatureState& policy,
    const DocumentPolicyFeatureInfoMap& feature_info_map) {
  net::structured_headers::Dictionary root;

  std::vector<std::pair<mojom::DocumentPolicyFeature, PolicyValue>>
      sorted_policy(policy.begin(), policy.end());
  std::sort(sorted_policy.begin(), sorted_policy.end(),
            [&](const auto& a, const auto& b) {
              const std::string& feature_a =
                  feature_info_map.at(a.first).feature_name;
              const std::string& feature_b =
                  feature_info_map.at(b.first).feature_name;
              return feature_a < feature_b;
            });

  for (const auto& policy_entry : sorted_policy) {
    const mojom::DocumentPolicyFeature feature = policy_entry.first;
    const std::string& feature_name = feature_info_map.at(feature).feature_name;
    const PolicyValue& value = policy_entry.second;

    root[feature_name] = net::structured_headers::ParameterizedMember(
        PolicyValueToItem(value), /* parameters */ {});
  }

  return net::structured_headers::SerializeDictionary(root);
}

// static
DocumentPolicyFeatureState DocumentPolicy::MergeFeatureState(
    const DocumentPolicyFeatureState& base_policy,
    const DocumentPolicyFeatureState& override_policy) {
  DocumentPolicyFeatureState result;
  auto i1 = base_policy.begin();
  auto i2 = override_policy.begin();

  // Because std::map is by default ordered in ascending order based on key
  // value, we can run 2 iterators simultaneously through both maps to merge
  // them.
  while (i1 != base_policy.end() || i2 != override_policy.end()) {
    if (i1 == base_policy.end()) {
      result.insert(*i2);
      i2++;
    } else if (i2 == override_policy.end()) {
      result.insert(*i1);
      i1++;
    } else {
      if (i1->first == i2->first) {
        const PolicyValue& base_value = i1->second;
        const PolicyValue& override_value = i2->second;
        // When policy value has strictness ordering e.g. boolean, take the
        // stricter one. In this case a.IsCompatibleWith(b) means a is eq or
        // stricter than b.
        // When policy value does not have strictness ordering, e.g. enum,
        // take override_value. In this case a.IsCompatibleWith(b) means
        // a != b.
        const PolicyValue& new_value =
            base_value.IsCompatibleWith(override_value) ? base_value
                                                        : override_value;
        result.emplace(i1->first, new_value);
        i1++;
        i2++;
      } else if (i1->first < i2->first) {
        result.insert(*i1);
        i1++;
      } else {
        result.insert(*i2);
        i2++;
      }
    }
  }

  return result;
}

bool DocumentPolicy::IsFeatureEnabled(
    mojom::DocumentPolicyFeature feature) const {
  mojom::PolicyValueType feature_type =
      GetDocumentPolicyFeatureInfoMap().at(feature).default_value.Type();
  return IsFeatureEnabled(feature,
                          PolicyValue::CreateMaxPolicyValue(feature_type));
}

bool DocumentPolicy::IsFeatureEnabled(
    mojom::DocumentPolicyFeature feature,
    const PolicyValue& threshold_value) const {
  return threshold_value.IsCompatibleWith(GetFeatureValue(feature));
}

PolicyValue DocumentPolicy::GetFeatureValue(
    mojom::DocumentPolicyFeature feature) const {
  return internal_feature_state_[static_cast<size_t>(feature)];
}

const std::optional<std::string> DocumentPolicy::GetFeatureEndpoint(
    mojom::DocumentPolicyFeature feature) const {
  auto endpoint_it = endpoint_map_.find(feature);
  if (endpoint_it != endpoint_map_.end()) {
    return endpoint_it->second;
  } else {
    return std::nullopt;
  }
}

void DocumentPolicy::UpdateFeatureState(
    const DocumentPolicyFeatureState& feature_state) {
  for (const auto& feature_and_value : feature_state) {
    internal_feature_state_[static_cast<size_t>(feature_and_value.first)] =
        feature_and_value.second;
  }
}

DocumentPolicy::DocumentPolicy(const DocumentPolicyFeatureState& header_policy,
                               const FeatureEndpointMap& endpoint_map,
                               const DocumentPolicyFeatureState& defaults)
    : endpoint_map_(endpoint_map) {
  // Fill the internal feature state with default value first,
  // and overwrite the value if it is specified in the header.
  UpdateFeatureState(defaults);
  UpdateFeatureState(header_policy);
}

// static
std::unique_ptr<DocumentPolicy> DocumentPolicy::CreateWithHeaderPolicy(
    const DocumentPolicyFeatureState& header_policy,
    const FeatureEndpointMap& endpoint_map,
    const DocumentPolicyFeatureState& defaults) {
  std::unique_ptr<DocumentPolicy> new_policy = base::WrapUnique(
      new DocumentPolicy(header_policy, endpoint_map, defaults));
  return new_policy;
}

// static
bool DocumentPolicy::IsPolicyCompatible(
    const DocumentPolicyFeatureState& required_policy,
    const DocumentPolicyFeatureState& incoming_policy) {
  for (const auto& required_entry : required_policy) {
    const auto& feature = required_entry.first;
    const auto& required_value = required_entry.second;
    // Use default value when incoming policy does not specify a value.
    const auto incoming_entry = incoming_policy.find(feature);
    const auto& incoming_value =
        incoming_entry != incoming_policy.end()
            ? incoming_entry->second
            : GetDocumentPolicyFeatureInfoMap().at(feature).default_value;

    if (!incoming_value.IsCompatibleWith(required_value))
      return false;
  }
  return true;
}

}  // namespace blink
