// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy_parser.h"

#include <algorithm>
#include <utility>
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"

namespace network {

namespace {
constexpr char kRequireCorp[] = "require-corp";
constexpr char kHeaderName[] = "cross-origin-embedder-policy";
constexpr char kReportOnlyHeaderName[] =
    "cross-origin-embedder-policy-report-only";

std::pair<mojom::CrossOriginEmbedderPolicyValue, base::Optional<std::string>>
Parse(base::StringPiece header_value) {
  constexpr auto kNone = mojom::CrossOriginEmbedderPolicyValue::kNone;
  using Item = net::structured_headers::Item;
  const auto item = net::structured_headers::ParseItem(header_value);
  if (!item || item->item.Type() != Item::kTokenType ||
      item->item.GetString() != kRequireCorp) {
    return std::make_pair(kNone, base::nullopt);
  }
  base::Optional<std::string> endpoint;
  auto it = std::find_if(item->params.cbegin(), item->params.cend(),
                         [](const std::pair<std::string, Item>& param) {
                           return param.first == "report-to";
                         });
  if (it != item->params.end() && it->second.Type() == Item::kStringType) {
    endpoint = it->second.GetString();
  }
  return std::make_pair(mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
                        std::move(endpoint));
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
