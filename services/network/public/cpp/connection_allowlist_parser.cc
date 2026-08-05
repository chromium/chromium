// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_allowlist_parser.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/types/optional_ref.h"
#include "components/url_pattern/simple_url_pattern_matcher.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/connection_allowlist.h"
#include "services/network/public/mojom/connection_allowlist.mojom-shared.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/origin_or_wildcard_header_value.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

constexpr char kResponseOriginToken[] = "response-origin";

constexpr char kReportToParam[] = "report-to";

constexpr char kRedirectsParam[] = "redirects";
constexpr char kWebRtcParam[] = "webrtc";

std::string* ParsePattern(
    net::structured_headers::Item& pattern,
    std::vector<mojom::ConnectionAllowlistIssue>& issues) {
  if (std::string* token = pattern.GetIfToken();
      token && *token == kResponseOriginToken) {
    return token;
  } else if (std::string* pattern_string = pattern.GetIfString();
             pattern_string && *pattern_string != kResponseOriginToken) {
    if (!url_pattern::SimpleUrlPatternMatcher::Create(*pattern_string,
                                                      /*base_url=*/nullptr)
             .has_value()) {
      issues.push_back(mojom::ConnectionAllowlistIssue::kInvalidUrlPattern);
      return nullptr;
    }
    return pattern_string;
  } else {
    issues.push_back(
        mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType);
    return nullptr;
  }
}

}  // namespace

ConnectionAllowlists ParseConnectionAllowlistsFromHeaders(
    const net::HttpResponseHeaders& headers,
    const GURL& response_url) {
  ConnectionAllowlists result;

  auto enforced_header = headers.GetNormalizedHeader("Connection-Allowlist");
  if (enforced_header) {
    result.enforced = ParseConnectionAllowlist(*enforced_header, response_url);
  }

  auto report_only_header =
      headers.GetNormalizedHeader("Connection-Allowlist-Report-Only");
  if (report_only_header) {
    result.report_only =
        ParseConnectionAllowlist(*report_only_header, response_url);
  }

  if (enforced_header || report_only_header) {
    result.response_url = response_url;
  }

  return result;
}

std::optional<ConnectionAllowlist> ParseConnectionAllowlist(
    const std::string& header_string,
    base::optional_ref<const GURL> response_url) {
  if (header_string.empty()) {
    return std::nullopt;
  }

  ConnectionAllowlist parsed;

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
  net::structured_headers::ParameterizedMember& inner_list = list->front();
  if (!inner_list.member_is_inner_list) {
    parsed.issues.push_back(mojom::ConnectionAllowlistIssue::kItemNotInnerList);
    return parsed;
  }

  // Process the list, adding patterns to the allowlist as we go. If we hit an
  // invalid value (e.g. not a `URLPattern` string or the `response-origin`
  // token, we'll ignore it and continue.
  for (auto& pattern : inner_list.member) {
    std::string* value = ParsePattern(pattern.item, parsed.issues);
    if (!value) {
      continue;
    }
    if (*value == kResponseOriginToken) {
      if (response_url) {
        parsed.allowlist.push_back(
            url::Origin::Create(*response_url).Serialize());
      } else {
        // Defer resolution: the origin to resolve against isn't known here
        // (e.g. parsing an iframe attribute in the renderer). The browser
        // resolves this against the response origin before enforcing.
        parsed.match_response_origin = true;
      }
    } else {
      parsed.allowlist.emplace_back(std::move(*value));
    }
  }

  // Process the list's parameters, ignoring any other than `report-to` or
  // special global tokens like `redirection-allowed` or `webrtc-allowed`.
  for (auto& [key, value] : inner_list.params) {
    if (key == kReportToParam) {
      if (std::string* token = value.GetIfToken()) {
        parsed.reporting_endpoint = std::move(*token);
      } else {
        parsed.issues.push_back(
            mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken);
      }
    } else if (key == kRedirectsParam) {
      const std::string* token = value.GetIfToken();
      parsed.redirect_behavior =
          (token && *token != "block")
              ? ConnectionAllowlist::RedirectBehavior::kAllow
              : ConnectionAllowlist::RedirectBehavior::kBlock;
    } else if (key == kWebRtcParam) {
      const std::string* token = value.GetIfToken();
      parsed.webrtc_behavior =
          (token && *token != "block")
              ? ConnectionAllowlist::WebRtcBehavior::kAllow
              : ConnectionAllowlist::WebRtcBehavior::kBlock;
    }
  }
  return parsed;
}

mojom::OriginOrWildcardHeaderValuePtr ParseAllowConnectionAllowlistFromHeader(
    const net::HttpResponseHeaders& headers) {
  std::optional<std::string> allow_connection_allowlist_from =
      headers.GetNormalizedHeader("Allow-Connection-Allowlist-From");
  if (!allow_connection_allowlist_from) {
    return nullptr;
  }

  std::string_view trimmed = base::TrimWhitespaceASCII(
      *allow_connection_allowlist_from, base::TRIM_ALL);

  if (trimmed == "*") {
    return mojom::OriginOrWildcardHeaderValue::NewAllowStar(true);
  }

  // Require an exact origin serialization, not merely a parsable URL: a URL can
  // carry a path, query, or userinfo that `url::Origin::Create` would silently
  // drop (e.g. `https://user@embedder.example/path`). Round-tripping through
  // the origin's serialization rejects anything that isn't already a bare
  // origin.
  url::Origin parsed_origin = url::Origin::Create(GURL(trimmed));
  if (parsed_origin.Serialize() != trimmed) {
    return mojom::OriginOrWildcardHeaderValue::NewErrorMessage(
        "The 'Allow-Connection-Allowlist-From' header contains neither '*' nor "
        "a valid origin.");
  }
  return mojom::OriginOrWildcardHeaderValue::NewOrigin(
      std::move(parsed_origin));
}

bool AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
    const url::Origin& request_origin,
    const GURL& response_url,
    const mojom::OriginOrWildcardHeaderValue* allow_connection_allowlist_from) {
  // Local schemes (about:, blob:, data:, filesystem:) inherit their embedder's
  // policies and do not initiate network connections on their own, so it is
  // always safe for the embedder to enforce its required allowlist on them.
  // This matches Fetch's notion of a "local scheme" and deliberately excludes
  // file:.
  if (response_url.SchemeIsLocal()) {
    return true;
  }

  if (!allow_connection_allowlist_from) {
    return false;
  }

  if (allow_connection_allowlist_from->is_allow_star()) {
    return true;
  }

  if (allow_connection_allowlist_from->is_origin() &&
      request_origin.IsSameOriginWith(
          allow_connection_allowlist_from->get_origin())) {
    return true;
  }

  return false;
}

void ReportConnectionAllowlistIssuesToDevtools(
    const ConnectionAllowlists& allowlists,
    const raw_ptr<mojom::DevToolsObserver> devtools_observer,
    const std::string& devtools_request_id,
    const GURL& request_url) {
  if (!devtools_observer || devtools_request_id.empty()) {
    return;
  }

  if (allowlists.enforced) {
    for (const mojom::ConnectionAllowlistIssue issue :
         allowlists.enforced->issues) {
      devtools_observer->OnConnectionAllowlistIssue(devtools_request_id,
                                                    request_url, issue);
    }
  }

  if (allowlists.report_only) {
    for (const mojom::ConnectionAllowlistIssue issue :
         allowlists.report_only->issues) {
      devtools_observer->OnConnectionAllowlistIssue(devtools_request_id,
                                                    request_url, issue);
    }
  }
}

}  // namespace network
