// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/common/web_engine_url_loader_throttle.h"

#include "base/strings/string_util.h"
#include "url/url_constants.h"

namespace {

// Returnns a string representing the URL stripped of query and ref.
std::string ClearUrlQueryRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements).spec();
}

void ApplyAddHeaders(network::ResourceRequest* request,
                     const mojom::UrlRequestRewriteAddHeadersPtr& add_headers) {
  request->headers.MergeFrom(add_headers->headers);
}

void ApplyRemoveHeader(
    network::ResourceRequest* request,
    const mojom::UrlRequestRewriteRemoveHeaderPtr& remove_header) {
  base::Optional<std::string> query_pattern = remove_header->query_pattern;
  if (query_pattern &&
      request->url.query().find(query_pattern.value()) == std::string::npos) {
    return;
  }

  request->headers.RemoveHeader(remove_header->header_name);
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

void ApplyRewrite(network::ResourceRequest* request,
                  const mojom::UrlRequestRewritePtr& rewrite) {
  switch (rewrite->which()) {
    case mojom::UrlRequestRewrite::Tag::ADD_HEADERS:
      ApplyAddHeaders(request, rewrite->get_add_headers());
      break;
    case mojom::UrlRequestRewrite::Tag::REMOVE_HEADER:
      ApplyRemoveHeader(request, rewrite->get_remove_header());
      break;
    case mojom::UrlRequestRewrite::Tag::SUBSTITUTE_QUERY_PATTERN:
      ApplySubstituteQueryPattern(request,
                                  rewrite->get_substitute_query_pattern());
      break;
    case mojom::UrlRequestRewrite::Tag::REPLACE_URL:
      ApplyReplaceUrl(request, rewrite->get_replace_url());
      break;
  }
}

bool HostMatches(const base::StringPiece& url_host,
                 const base::StringPiece& rule_host) {
  const base::StringPiece kWildcard("*.");
  if (base::StartsWith(rule_host, kWildcard, base::CompareCase::SENSITIVE)) {
    // |rule_host| starts with a wildcard (e.g. "*.test.xyz"). Check if
    // |url_host| ends with ".test.xyz". The "." character is included here to
    // prevent accidentally matching "notatest.xyz".
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

void ApplyRule(network::ResourceRequest* request,
               const mojom::UrlRequestRewriteRulePtr& rule) {
  const GURL& url = request->url;

  if (rule->hosts_filter) {
    bool found = false;
    for (const base::StringPiece host : rule->hosts_filter.value()) {
      if ((found = HostMatches(url.host(), host)))
        break;
    }
    if (!found)
      return;
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
      return;
  }

  for (const auto& rewrite : rule->rewrites) {
    ApplyRewrite(request, rewrite);
  }
}

void ApplyRules(network::ResourceRequest* request,
                const std::vector<mojom::UrlRequestRewriteRulePtr>& rules) {
  for (const auto& rule : rules) {
    ApplyRule(request, rule);
  }
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
  if (cached_rules)
    ApplyRules(request, cached_rules->data);
  *defer = false;
}

bool WebEngineURLLoaderThrottle::makes_unsafe_redirect() {
  // WillStartRequest() does not make cross-scheme redirects.
  return false;
}
