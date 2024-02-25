// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy_parser.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"

namespace network {

namespace {
constexpr char kHeaderName[] = "cross-origin-embedder-policy";
constexpr char kReportOnlyHeaderName[] =
    "cross-origin-embedder-policy-report-only";

// [spec]: https://html.spec.whatwg.org/C/#obtain-an-embedder-policy
std::pair<mojom::CrossOriginEmbedderPolicyValue, std::optional<std::string>>
Parse(std::string_view header_value) {
  using Item = net::structured_headers::Item;
  const auto item = net::structured_headers::ParseItem(header_value);
  if (!item || item->item.Type() != net::structured_headers::Item::kTokenType) {
    return {
        mojom::CrossOriginEmbedderPolicyValue::kNone,
        std::nullopt,
    };
  }

  std::optional<std::string> endpoint;
  for (const auto& it : item->params) {
    if (it.first == "report-to" && it.second.Type() == Item::kStringType)
      endpoint = it.second.GetString();
  }

  if (item->item.GetString() == "require-corp") {
    return {
        mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
        std::move(endpoint),
    };
  }

  if (item->item.GetString() == "credentialless") {
    return {
        mojom::CrossOriginEmbedderPolicyValue::kCredentialless,
        std::move(endpoint),
    };
  }

  return {
      mojom::CrossOriginEmbedderPolicyValue::kNone,
      std::nullopt,
  };
}

}  // namespace

CrossOriginEmbedderPolicy ParseCrossOriginEmbedderPolicy(
    const net::HttpResponseHeaders& headers) {
  CrossOriginEmbedderPolicy coep;
  std::string header_value;
  if (headers.GetNormalizedHeader(kHeaderName, &header_value)) {
    std::tie(coep.value, coep.reporting_endpoint) = Parse(header_value);
  }
  if (headers.GetNormalizedHeader(kReportOnlyHeaderName, &header_value)) {
    std::tie(coep.report_only_value, coep.report_only_reporting_endpoint) =
        Parse(header_value);
  }
  return coep;
}

}  // namespace network
