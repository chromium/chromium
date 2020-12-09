// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_parsed_header.h"

#include "base/containers/contains.h"
#include "net/http/structured_headers.h"

namespace network {

namespace sh = net::structured_headers;

OriginPolicyParsedHeader::OriginPolicyParsedHeader(
    const std::vector<OriginPolicyAllowedValue>& allowed,
    const base::Optional<OriginPolicyPreferredValue>& preferred)
    : allowed_(allowed), preferred_(preferred) {
  DCHECK(!allowed_.empty() || preferred_.has_value());
}

OriginPolicyParsedHeader::~OriginPolicyParsedHeader() = default;

OriginPolicyParsedHeader::OriginPolicyParsedHeader(
    const OriginPolicyParsedHeader&) = default;

// https://wicg.github.io/origin-policy/#parse-an-origin-policy-header
base::Optional<OriginPolicyParsedHeader> OriginPolicyParsedHeader::FromString(
    const std::string& string) {
  base::Optional<sh::Dictionary> parsed_header_opt =
      sh::ParseDictionary(string);
  if (!parsed_header_opt) {
    return base::nullopt;
  }

  const sh::Dictionary& parsed_header = *parsed_header_opt;

  std::vector<OriginPolicyAllowedValue> allowed;

  if (parsed_header.contains("allowed")) {
    if (!parsed_header.at("allowed").member_is_inner_list) {
      return base::nullopt;
    }

    const std::vector<sh::ParameterizedItem>& raw_allowed_list =
        parsed_header.at("allowed").member;
    for (const auto& parameterized_item : raw_allowed_list) {
      base::Optional<OriginPolicyAllowedValue> result;

      const sh::Item& item = parameterized_item.item;
      if (item.is_string()) {
        const std::string& string = item.GetString();
        if (string.empty()) {
          return base::nullopt;
        }
        result = OriginPolicyAllowedValue::FromString(string);
      } else if (item.is_token()) {
        const std::string& string = item.GetString();
        if (string == "null") {
          result = OriginPolicyAllowedValue::Null();
        } else if (string == "latest") {
          result = OriginPolicyAllowedValue::Latest();
        }
      } else {
        return base::nullopt;
      }

      if (result && !base::Contains(allowed, *result)) {
        allowed.push_back(*result);
      }
    }
  }

  base::Optional<OriginPolicyPreferredValue> preferred;
  if (parsed_header.contains("preferred")) {
    if (parsed_header.at("preferred").member_is_inner_list) {
      return base::nullopt;
    }

    const sh::Item& item = parsed_header.at("preferred").member[0].item;
    if (item.is_string()) {
      const std::string& string = item.GetString();
      if (string.empty()) {
        return base::nullopt;
      }
      preferred = OriginPolicyPreferredValue::FromString(string);
    } else if (item.is_token()) {
      const std::string& string = item.GetString();
      if (string == "latest-from-network") {
        preferred = OriginPolicyPreferredValue::LatestFromNetwork();
      }
    } else {
      return base::nullopt;
    }
  }

  if (allowed.empty() && !preferred.has_value()) {
    return base::nullopt;
  }

  return OriginPolicyParsedHeader(allowed, preferred);
}

}  // namespace network
