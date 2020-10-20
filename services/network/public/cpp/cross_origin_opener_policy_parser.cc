// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/features.h"
namespace network {

namespace {

// Const definition of the strings involved in the header parsing.
constexpr char kCrossOriginOpenerPolicyHeader[] = "Cross-Origin-Opener-Policy";
constexpr char kCrossOriginOpenerPolicyHeaderReportOnly[] =
    "Cross-Origin-Opener-Policy-Report-Only";
constexpr char kSameOrigin[] = "same-origin";
constexpr char kSameOriginAllowPopups[] = "same-origin-allow-popups";
constexpr char kReportTo[] = "report-to";

std::pair<mojom::CrossOriginOpenerPolicyValue, base::Optional<std::string>>
ParseHeader(base::StringPiece header_value) {
  using Item = net::structured_headers::Item;
  // Default to kUnsafeNone for all malformed values and "unsafe-none"
  mojom::CrossOriginOpenerPolicyValue coop_value =
      mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;
  base::Optional<std::string> endpoint;
  const auto item = net::structured_headers::ParseItem(header_value);
  if (item && item->item.is_token()) {
    const auto& policy_item = item->item.GetString();
    if (policy_item == kSameOrigin)
      coop_value = mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
    if (policy_item == kSameOriginAllowPopups)
      coop_value = mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
    auto it = std::find_if(item->params.cbegin(), item->params.cend(),
                           [](const std::pair<std::string, Item>& param) {
                             return param.first == kReportTo;
                           });
    if (it != item->params.end() && it->second.is_string()) {
      endpoint = it->second.GetString();
    }
  }
  return std::make_pair(coop_value, std::move(endpoint));
}

}  // namespace

CrossOriginOpenerPolicy ParseCrossOriginOpenerPolicy(
    const net::HttpResponseHeaders& headers,
    const CrossOriginEmbedderPolicy& coep) {
  CrossOriginOpenerPolicy coop;

  // This is the single line of code disabling COOP globally.
  if (!base::FeatureList::IsEnabled(features::kCrossOriginOpenerPolicy))
    return coop;

  std::string header_value;
  if (headers.GetNormalizedHeader(kCrossOriginOpenerPolicyHeader,
                                  &header_value)) {
    std::tie(coop.value, coop.reporting_endpoint) = ParseHeader(header_value);
    if (coop.value == mojom::CrossOriginOpenerPolicyValue::kSameOrigin &&
        coep.value == mojom::CrossOriginEmbedderPolicyValue::kRequireCorp)
      coop.value = mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
  } else if (base::FeatureList::IsEnabled(
                 features::kCrossOriginOpenerPolicyByDefault)) {
    coop.value = mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  }
  if (headers.GetNormalizedHeader(kCrossOriginOpenerPolicyHeaderReportOnly,
                                  &header_value)) {
    std::tie(coop.report_only_value, coop.report_only_reporting_endpoint) =
      ParseHeader(header_value);
    if (coop.report_only_value ==
            mojom::CrossOriginOpenerPolicyValue::kSameOrigin &&
        (coep.value == mojom::CrossOriginEmbedderPolicyValue::kRequireCorp ||
         coep.report_only_value ==
             mojom::CrossOriginEmbedderPolicyValue::kRequireCorp)) {
      coop.report_only_value =
          mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
    }
  }
  return coop;
}

}  // namespace network
