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
  auto item = net::structured_headers::ParseItem(header_value);
  const std::string* token = item ? item->item.GetIfToken() : nullptr;
  if (!token) {
    return {
        mojom::DocumentIsolationPolicyValue::kNone,
        std::nullopt,
    };
  }

  std::optional<std::string> endpoint;
  for (auto& [key, value] : item->params) {
    if (std::string* str = value.GetIfString(); key == "report-to" && str) {
      endpoint = std::move(*str);
    }
  }

  if (*token == "isolate-and-require-corp") {
    return {
        mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp,
        std::move(endpoint),
    };
  }

  if (*token == "isolate-and-credentialless") {
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
  if (std::optional<std::string> header_value =
          headers.GetNormalizedHeader(kHeaderName)) {
    std::tie(dip.value, dip.reporting_endpoint) = Parse(*header_value);
  }
  if (std::optional<std::string> header_value =
          headers.GetNormalizedHeader(kReportOnlyHeaderName)) {
    std::tie(dip.report_only_value, dip.report_only_reporting_endpoint) =
        Parse(*header_value);
  }
  return dip;
}

}  // namespace network
