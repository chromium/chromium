// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/permissions_policy/document_policy_parser.h"

#include "net/http/structured_headers.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom-blink.h"

namespace blink {
namespace {

constexpr const char* kReportTo = "report-to";
constexpr const char* kNone = "none";

const char* PolicyValueTypeToString(mojom::blink::PolicyValueType type) {
  switch (type) {
    case mojom::blink::PolicyValueType::kNull:
      return "null";
    case mojom::blink::PolicyValueType::kBool:
      return "boolean";
    case mojom::blink::PolicyValueType::kDecDouble:
      return "double";
    case mojom::blink::PolicyValueType::kEnum:
      return "enum";
  }
}

std::optional<PolicyValue> ItemToPolicyValue(
    const net::structured_headers::Item& item,
    mojom::blink::PolicyValueType type) {
  switch (type) {
    case mojom::blink::PolicyValueType::kBool: {
      if (item.is_boolean()) {
        return PolicyValue::CreateBool(item.GetBoolean());
      } else {
        return std::nullopt;
      }
    }
    case mojom::blink::PolicyValueType::kDecDouble:
      switch (item.Type()) {
        case net::structured_headers::Item::ItemType::kIntegerType:
          return PolicyValue::CreateDecDouble(
              static_cast<double>(item.GetInteger()));
        case net::structured_headers::Item::ItemType::kDecimalType:
          return PolicyValue::CreateDecDouble(item.GetDecimal());
        default:
          return std::nullopt;
      }
    default:
      return std::nullopt;
  }
}

std::optional<std::string> ItemToString(
    const net::structured_headers::Item& item) {
  if (item.Type() != net::structured_headers::Item::ItemType::kTokenType)
    return std::nullopt;
  return item.GetString();
}

struct ParsedFeature {
  mojom::blink::DocumentPolicyFeature feature;
  PolicyValue policy_value;
  std::optional<std::string> endpoint_group;
};

std::optional<ParsedFeature> ParseFeature(
    const net::structured_headers::DictionaryMember& directive,
    const DocumentPolicyNameFeatureMap& name_feature_map,
    const DocumentPolicyFeatureInfoMap& feature_info_map,
    PolicyParserMessageBuffer& logger) {
  ParsedFeature parsed_feature;

  const std::string& feature_name = directive.first;
  if (directive.second.member_is_inner_list) {
    logger.Warn(
        String::Format("Parameter for feature %s should be single item, but "
                       "get list of items(length=%d).",
                       feature_name.c_str(),
                       static_cast<uint32_t>(directive.second.member.size())));
    return std::nullopt;
  }

  // Parse feature_name string to DocumentPolicyFeature.
  auto feature_iter = name_feature_map.find(feature_name);
  if (feature_iter != name_feature_map.end()) {
    parsed_feature.feature = feature_iter->second;
  } else {
    logger.Warn(String::Format("Unrecognized document policy feature name %s.",
                               feature_name.c_str()));
    return std::nullopt;
  }

  auto expected_policy_value_type =
      feature_info_map.at(parsed_feature.feature).default_value.Type();
  const net::structured_headers::Item& item =
      directive.second.member.front().item;
  std::optional<PolicyValue> policy_value =
      ItemToPolicyValue(item, expected_policy_value_type);
  if (!policy_value) {
    logger.Warn(String::Format(
        "Parameter for feature %s should be %s, not %s.", feature_name.c_str(),
        PolicyValueTypeToString(expected_policy_value_type),
        net::structured_headers::ItemTypeToString(item.Type()).data()));
    return std::nullopt;
  }
  parsed_feature.policy_value = *policy_value;

  for (const auto& param : directive.second.params) {
    const std::string& param_name = param.first;
    // Handle "report-to" param. "report-to" is an optional param for
    // Document-Policy header that specifies the endpoint group that the policy
    // should send report to. If left unspecified, no report will be send upon
    // policy violation.
    if (param_name == kReportTo) {
      parsed_feature.endpoint_group = ItemToString(param.second);
      if (!parsed_feature.endpoint_group) {
        logger.Warn(String::Format(
            "\"report-to\" parameter should be a token in feature %s.",
            feature_name.c_str()));
        return std::nullopt;
      }
    } else {
      // Unrecognized param.
      logger.Warn(
          String::Format("Unrecognized parameter name %s for feature %s.",
                         param_name.c_str(), feature_name.c_str()));
    }
  }

  return parsed_feature;
}

// Apply |default_endpoint| to given |parsed_policy|.
void ApplyDefaultEndpoint(DocumentPolicy::ParsedDocumentPolicy& parsed_policy,
                          const std::string& default_endpoint) {
  DocumentPolicy::FeatureEndpointMap& endpoint_map = parsed_policy.endpoint_map;

  if (!default_endpoint.empty()) {
    // Fill |default_endpoint| to all feature entry whose |endpoint_group|
    // is missing.
    for (const auto& feature_and_value : parsed_policy.feature_state) {
      mojom::blink::DocumentPolicyFeature feature = feature_and_value.first;

      if (endpoint_map.find(feature) == endpoint_map.end())
        endpoint_map.emplace(feature, default_endpoint);
    }
  }

  // Remove |endpoint_group| for feature entry if its |endpoint_group|
  // is "none".
  // Note: if |default_endpoint| is "none", all "none" items are filtered out
  // here. it would be equivalent to doing nothing.
  for (auto iter = endpoint_map.begin(); iter != endpoint_map.end();) {
    if (iter->second == kNone) {
      iter = endpoint_map.erase(iter);
    } else {
      ++iter;
    }
  }
}

}  // namespace

// static
std::optional<DocumentPolicy::ParsedDocumentPolicy> DocumentPolicyParser::Parse(
    const String& policy_string,
    PolicyParserMessageBuffer& logger) {
  if (policy_string.empty())
    return std::make_optional<DocumentPolicy::ParsedDocumentPolicy>({});

  return ParseInternal(policy_string, GetDocumentPolicyNameFeatureMap(),
                       GetDocumentPolicyFeatureInfoMap(),
                       GetAvailableDocumentPolicyFeatures(), logger);
}

// static
std::optional<DocumentPolicy::ParsedDocumentPolicy>
DocumentPolicyParser::ParseInternal(
    const String& policy_string,
    const DocumentPolicyNameFeatureMap& name_feature_map,
    const DocumentPolicyFeatureInfoMap& feature_info_map,
    const DocumentPolicyFeatureSet& available_features,
    PolicyParserMessageBuffer& logger) {
  auto root = net::structured_headers::ParseDictionary(policy_string.Ascii());
  if (!root) {
    logger.Error(
        "Parse of document policy failed because of errors reported by "
        "structured header parser.");
    return std::nullopt;
  }

  DocumentPolicy::ParsedDocumentPolicy parse_result;
  std::string default_endpoint = "";
  for (const net::structured_headers::DictionaryMember& directive :
       root.value()) {
    std::optional<ParsedFeature> parsed_feature_option =
        ParseFeature(directive, name_feature_map, feature_info_map, logger);
    // If a feature fails parsing, ignore the entry.
    if (!parsed_feature_option)
      continue;

    ParsedFeature parsed_feature = *parsed_feature_option;

    if (parsed_feature.feature ==
        mojom::blink::DocumentPolicyFeature::kDefault) {
      if (parsed_feature.endpoint_group)
        default_endpoint = *parsed_feature.endpoint_group;
      continue;
    }

    // If feature is not available, i.e. not enabled, ignore the entry.
    if (available_features.find(parsed_feature.feature) ==
        available_features.end())
      continue;

    parse_result.feature_state.emplace(parsed_feature.feature,
                                       std::move(parsed_feature.policy_value));
    if (parsed_feature.endpoint_group) {
      parse_result.endpoint_map.emplace(parsed_feature.feature,
                                        *parsed_feature.endpoint_group);
    }
  }

  ApplyDefaultEndpoint(parse_result, default_endpoint);

  return parse_result;
}

}  // namespace blink
