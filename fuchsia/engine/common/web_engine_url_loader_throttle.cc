// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/common/web_engine_url_loader_throttle.h"

#include <string>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "fuchsia/engine/common/cors_exempt_headers.h"
#include "url/url_constants.h"

namespace {

// Returnns a string representing the URL stripped of query and ref.
std::string ClearUrlQueryRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements).spec();
}

void ApplySubstituteQueryPattern(
    network::ResourceRequest* request,
    const mojom::UrlRequestRewriteSubstituteQueryPatternPtr&
        substitute_query_pattern) {
  std::string url_query = request->url.query();

  base::ReplaceSubstringsAfterOffset(&url_query, 0,
                                     substitute_query_pattern->pattern,
                                     substitute_query_pattern->substitution);

  GURL::Replacements replacements;
  replacements.SetQueryStr(url_query);
  request->url = request->url.ReplaceComponents(replacements);
}

void ApplyReplaceUrl(network::ResourceRequest* request,
                     const mojom::UrlRequestRewriteReplaceUrlPtr& replace_url) {
  if (!base::EndsWith(ClearUrlQueryRef(request->url),
                      replace_url->url_ends_with, base::CompareCase::SENSITIVE))
    return;

  GURL new_url = replace_url->new_url;
  if (new_url.SchemeIs(url::kDataScheme)) {
    request->url = new_url;
    return;
  }

  if (new_url.has_scheme() &&
      new_url.scheme().compare(request->url.scheme()) != 0) {
    // No cross-scheme redirect allowed.
    return;
  }

  GURL::Replacements replacements;
  std::string host = new_url.host();
  replacements.SetHostStr(host);
  std::string port = new_url.port();
  replacements.SetPortStr(port);
  std::string path = new_url.path();
  replacements.SetPathStr(path);

  request->url = request->url.ReplaceComponents(replacements);
}

void ApplyRemoveHeader(
    network::ResourceRequest* request,
    const mojom::UrlRequestRewriteRemoveHeaderPtr& remove_header) {
  base::Optional<std::string> query_pattern = remove_header->query_pattern;
  if (query_pattern &&
      request->url.query().find(query_pattern.value()) == std::string::npos) {
    // Per the FIDL API, the header should be removed if there is no query
    // pattern or if the pattern matches. Neither is true here.
    return;
  }

  request->headers.RemoveHeader(remove_header->header_name);
  request->cors_exempt_headers.RemoveHeader(remove_header->header_name);
}

void ApplyAppendToQuery(
    network::ResourceRequest* request,
    const mojom::UrlRequestRewriteAppendToQueryPtr& append_to_query) {
  std::string url_query;
  if (request->url.has_query() && !request->url.query().empty())
    url_query = request->url.query() + "&";
  url_query += append_to_query->query;

  GURL::Replacements replacements;
  replacements.SetQueryStr(url_query);
  request->url = request->url.ReplaceComponents(replacements);
}

bool HostMatches(const base::StringPiece& url_host,
                 const base::StringPiece& rule_host) {
  const base::StringPiece kWildcard("*.");
  if (base::StartsWith(rule_host, kWildcard, base::CompareCase::SENSITIVE)) {
    if (base::EndsWith(url_host, rule_host.substr(1),
                       base::CompareCase::SENSITIVE)) {
      return true;
    }

    // Check |url_host| is exactly |rule_host| without the wildcard. i.e. if
    // |rule_host| is "*.test.xyz", check |url_host| is exactly "test.xyz".
    return base::CompareCaseInsensitiveASCII(url_host, rule_host.substr(2)) ==
           0;
  }
  return base::CompareCaseInsensitiveASCII(url_host, rule_host) == 0;
}

// Returns true if the host and scheme filters defined in |rule| match
// |request|.
bool RuleFiltersMatchRequest(network::ResourceRequest* request,
                             const mojom::UrlRequestRulePtr& rule) {
  const GURL& url = request->url;

  if (rule->hosts_filter) {
    bool found = false;
    for (const base::StringPiece host : rule->hosts_filter.value()) {
      if ((found = HostMatches(url.host(), host)))
        break;
    }
    if (!found)
      return false;
  }

  if (rule->schemes_filter) {
    bool found = false;
    for (const auto& scheme : rule->schemes_filter.value()) {
      if (url.scheme().compare(scheme) == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }

  return true;
}

// Returns true if |request| is either allowed or left unblocked by any rules.
bool IsRequestAllowed(
    network::ResourceRequest* request,
    const WebEngineURLLoaderThrottle::UrlRequestRewriteRules& rules) {
  for (const auto& rule : rules.data) {
    if (rule->actions.size() != 1)
      continue;

    if (rule->actions[0]->which() != mojom::UrlRequestAction::Tag::POLICY)
      continue;

    if (!RuleFiltersMatchRequest(request, rule))
      continue;

    switch (rule->actions[0]->get_policy()) {
      case mojom::UrlRequestAccessPolicy::kAllow:
        return true;
      case mojom::UrlRequestAccessPolicy::kDeny:
        return false;
    }
  }

  return true;
}

}  // namespace

WebEngineURLLoaderThrottle::WebEngineURLLoaderThrottle(
    CachedRulesProvider* cached_rules_provider)
    : cached_rules_provider_(cached_rules_provider) {
  DCHECK(cached_rules_provider);
}

WebEngineURLLoaderThrottle::~WebEngineURLLoaderThrottle() = default;

void WebEngineURLLoaderThrottle::DetachFromCurrentSequence() {}

void WebEngineURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules = cached_rules_provider_->GetCachedRules();
  // |cached_rules| may be empty if no rule was ever sent to WebEngine.
  if (cached_rules) {
    if (!IsRequestAllowed(request, *cached_rules)) {
      delegate_->CancelWithError(net::ERR_ABORTED,
                                 "Resource load blocked by embedder policy.");
      return;
    }

    for (const auto& rule : cached_rules->data)
      ApplyRule(request, rule);
  }
  *defer = false;
}

bool WebEngineURLLoaderThrottle::makes_unsafe_redirect() {
  // WillStartRequest() does not make cross-scheme redirects.
  return false;
}

void WebEngineURLLoaderThrottle::ApplyRule(
    network::ResourceRequest* request,
    const mojom::UrlRequestRulePtr& rule) {
  if (!RuleFiltersMatchRequest(request, rule))
    return;

  for (const auto& rewrite : rule->actions)
    ApplyRewrite(request, rewrite);
}

void WebEngineURLLoaderThrottle::ApplyRewrite(
    network::ResourceRequest* request,
    const mojom::UrlRequestActionPtr& rewrite) {
  switch (rewrite->which()) {
    case mojom::UrlRequestAction::Tag::ADD_HEADERS:
      ApplyAddHeaders(request, rewrite->get_add_headers());
      return;
    case mojom::UrlRequestAction::Tag::REMOVE_HEADER:
      ApplyRemoveHeader(request, rewrite->get_remove_header());
      return;
    case mojom::UrlRequestAction::Tag::SUBSTITUTE_QUERY_PATTERN:
      ApplySubstituteQueryPattern(request,
                                  rewrite->get_substitute_query_pattern());
      return;
    case mojom::UrlRequestAction::Tag::REPLACE_URL:
      ApplyReplaceUrl(request, rewrite->get_replace_url());
      return;
    case mojom::UrlRequestAction::Tag::APPEND_TO_QUERY:
      ApplyAppendToQuery(request, rewrite->get_append_to_query());
      return;
    case mojom::UrlRequestAction::Tag::POLICY:
      // "Policy" is interpreted elsewhere; it is a no-op for rewriting.
      return;
  }
  NOTREACHED();  // Invalid enum value.
}

void WebEngineURLLoaderThrottle::ApplyAddHeaders(
    network::ResourceRequest* request,
    const mojom::UrlRequestRewriteAddHeadersPtr& add_headers) {
  // Bucket each |header| into the regular/CORS-compliant header list or the
  // CORS-exempt header list.
  for (const auto& header : add_headers->headers.GetHeaderVector()) {
    if (IsHeaderCorsExempt(header.key)) {
      request->cors_exempt_headers.SetHeader(header.key, header.value);
    } else {
      request->headers.SetHeader(header.key, header.value);
    }
  }
}
