// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/integrity_policy_parser.h"

#include "base/strings/cstring_view.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/integrity_policy.h"

namespace network {
namespace {

void HandleKeyValue(base::cstring_view key,
                    const net::structured_headers::ParameterizedMember& value,
                    base::cstring_view header_name,
                    IntegrityPolicy& policy) {
  if (!value.member_is_inner_list) {
    policy.parsing_errors.emplace_back(
        base::StringPrintf("The %s value is not a list.", header_name.c_str()));
    return;
  }
  for (const auto& parameter : value.member) {
    if (!parameter.item.is_token()) {
      policy.parsing_errors.emplace_back(base::StringPrintf(
          "The %s item '%s' is not a token. "
          "Did you accidentally add it as a string?",
          header_name.c_str(), parameter.item.GetString().c_str()));
      continue;
    }
    const base::cstring_view parameter_value = parameter.item.GetString();
    if (key == "blocked-destinations") {
      if (parameter_value == "script") {
        policy.blocked_destinations.emplace_back(
            mojom::IntegrityPolicy_Destination::kScript);
      } else {
        policy.parsing_errors.emplace_back(
            base::StringPrintf("The %s destination '%s' is not supported.",
                               header_name.c_str(), parameter_value.c_str()));
      }
    } else if (key == "sources") {
      if (parameter_value == "inline") {
        policy.sources.emplace_back(mojom::IntegrityPolicy_Source::kInline);
      } else {
        policy.parsing_errors.emplace_back(
            base::StringPrintf("The %s source '%s' is not supported.",
                               header_name.c_str(), parameter_value.c_str()));
      }
    } else if (key == "endpoints") {
      policy.endpoints.emplace_back(parameter_value);
    } else {
      policy.parsing_errors.emplace_back(base::StringPrintf(
          "Unrecognized %s in %s header.", key.c_str(), header_name.c_str()));
    }
  }
}

}  // namespace

IntegrityPolicy ParseIntegrityPolicyFromHeaders(
    const net::HttpResponseHeaders& headers,
    IntegrityPolicyHeaderType type) {
  CHECK(
      base::FeatureList::IsEnabled(network::features::kIntegrityPolicyScript));
  IntegrityPolicy parsed_policy;
  const std::string header_name = (type == IntegrityPolicyHeaderType::kEnforce)
                                      ? "Integrity-Policy"
                                      : "Integrity-Policy-Report-Only";

  const std::string integrity_policy_header =
      headers.GetNormalizedHeader(header_name).value_or("");
  if (integrity_policy_header.empty()) {
    return parsed_policy;
  }

  std::optional<net::structured_headers::Dictionary>
      integrity_policy_dictionary =
          net::structured_headers::ParseDictionary(integrity_policy_header);
  if (!integrity_policy_dictionary) {
    parsed_policy.parsing_errors.emplace_back(base::StringPrintf(
        "The %s value \"%s\" is not a dictionary.", header_name.c_str(),
        integrity_policy_header.c_str()));
    return parsed_policy;
  }

  bool has_sources_key = false;
  // Loop through the policy dictionary
  //
  // https://datatracker.ietf.org/doc/html/rfc9421#section-4-4
  for (const net::structured_headers::DictionaryMember& policy_entry :
       integrity_policy_dictionary.value()) {
    if (policy_entry.first == "sources") {
      has_sources_key = true;
    }
    HandleKeyValue(policy_entry.first, policy_entry.second, header_name,
                   parsed_policy);
  }
  if (!has_sources_key) {
    // If the `sources` key is missing from the header, add "inline" as the
    // default. That would enable us to add future integrity metadata sources,
    // without forcing developers to explicitly add the source today, when only
    // one integrity metadata source exists.
    parsed_policy.sources.emplace_back(mojom::IntegrityPolicy_Source::kInline);
  }
  return parsed_policy;
}

}  // namespace network
