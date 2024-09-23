// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/document_isolation_policy_parser.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/document_isolation_policy.h"

namespace network {

namespace {
constexpr char kHeaderName[] = "document-isolation-policy";
constexpr char kReportOnlyHeaderName[] =
    "document-isolation-policy-report-only";

std::pair<mojom::DocumentIsolationPolicyValue, std::optional<std::string>>
Parse(std::string_view header_value) {
  using Item = net::structured_headers::Item;
  const auto item = net::structured_headers::ParseItem(header_value);
  if (!item || item->item.Type() != net::structured_headers::Item::kTokenType) {
    return {
        mojom::DocumentIsolationPolicyValue::kNone,
        std::nullopt,
    };
  }

  std::optional<std::string> endpoint;
  for (const auto& it : item->params) {
    if (it.first == "report-to" && it.second.Type() == Item::kStringType) {
      endpoint = it.second.GetString();
    }
  }

  if (item->item.GetString() == "isolate-and-require-corp") {
    return {
        mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
        std::move(endpoint),
    };
  }

  if (item->item.GetString() == "isolate-and-credentialless") {
    return {
        mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless,
        std::move(endpoint),
    };
  }

  return {
      mojom::DocumentIsolationPolicyValue::kNone,
      std::nullopt,
  };
}

}  // namespace

DocumentIsolationPolicy ParseDocumentIsolationPolicy(
    const net::HttpResponseHeaders& headers) {
  DocumentIsolationPolicy dip;
  std::string header_value;
  if (headers.GetNormalizedHeader(kHeaderName, &header_value)) {
    std::tie(dip.value, dip.reporting_endpoint) = Parse(header_value);
  }
  if (headers.GetNormalizedHeader(kReportOnlyHeaderName, &header_value)) {
    std::tie(dip.report_only_value, dip.report_only_reporting_endpoint) =
        Parse(header_value);
  }
  return dip;
}

}  // namespace network
