// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_allowlist_parser.h"

#include "net/http/structured_headers.h"
#include "services/network/public/cpp/connection_allowlist.h"
#include "services/network/public/mojom/connection_allowlist.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

constexpr char kResponseOriginToken[] = "response-origin";

std::optional<std::string> ParsePattern(
    const net::structured_headers::ParameterizedItem& pattern,
    std::vector<mojom::ConnectionAllowlistIssue>& issues) {
  if (pattern.item.is_token() &&
      pattern.item.GetString() == kResponseOriginToken) {
    return kResponseOriginToken;
  } else if (pattern.item.is_string() &&
             pattern.item.GetString() != kResponseOriginToken) {
    // TODO(mkwst): Validate the string as a `URLPattern`.
    return pattern.item.GetString();
  } else {
    issues.push_back(
        mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType);
    return std::nullopt;
  }
}

std::optional<ConnectionAllowlist> ParseHeader(const std::string& header_string,
                                               const GURL& response_url) {
  if (header_string.empty()) {
    return std::nullopt;
  }

  ConnectionAllowlist parsed;
  std::string serialized_origin = url::Origin::Create(response_url).Serialize();

  // Parse the header as a List.
  std::optional<net::structured_headers::List> list =
      net::structured_headers::ParseList(header_string);
  if (!list || list->empty()) {
    parsed.issues.push_back(mojom::ConnectionAllowlistIssue::kInvalidHeader);
    return parsed;
  }

  // If there's more than one entry in the list, we're only going to process
  // the first one. Flag it for devtools, but continue processing.
  if (list->size() > 1) {
    parsed.issues.push_back(mojom::ConnectionAllowlistIssue::kMoreThanOneList);
  }

  // The single item we process must be an InnerList.
  const net::structured_headers::ParameterizedMember& inner_list = (*list)[0];
  if (!inner_list.member_is_inner_list) {
    parsed.issues.push_back(mojom::ConnectionAllowlistIssue::kItemNotInnerList);
    return parsed;
  }

  // Process the list, adding patterns to the allowlist as we go. If we hit an
  // invalid value (e.g. not a `URLPattern` string or the `response-origin`
  // token, we'll ignore it and continue.
  for (const auto& pattern : inner_list.member) {
    std::optional<std::string> value = ParsePattern(pattern, parsed.issues);
    if (!value) {
      continue;
    }
    if (*value == kResponseOriginToken) {
      parsed.allowlist.push_back(serialized_origin);
    } else {
      parsed.allowlist.push_back(*value);
    }
  }

  // Process the list's parameters, ignoring any other than `report-to`.
  for (const auto& param : inner_list.params) {
    if (param.first == "report-to") {
      if (param.second.is_token()) {
        parsed.reporting_endpoint = param.second.GetString();
      } else {
        parsed.issues.push_back(
            mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken);
      }
    }
  }
  return parsed;
}

}  // namespace

ConnectionAllowlists ParseConnectionAllowlistsFromHeaders(
    const net::HttpResponseHeaders& headers,
    const GURL& response_url) {
  ConnectionAllowlists result;

  auto enforced_header = headers.GetNormalizedHeader("Connection-Allowlist");
  if (enforced_header) {
    result.enforced = ParseHeader(*enforced_header, response_url);
  }

  auto report_only_header =
      headers.GetNormalizedHeader("Connection-Allowlist-Report-Only");
  if (report_only_header) {
    result.report_only = ParseHeader(*report_only_header, response_url);
  }

  return result;
}

}  // namespace network
