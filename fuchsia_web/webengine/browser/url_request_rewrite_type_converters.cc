// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/url_request_rewrite_type_converters.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "fuchsia_web/common/string_util.h"
#include "net/base/url_util.h"

namespace {

std::string NormalizeHost(std::string_view host) {
  return GURL(base::StrCat({url::kHttpScheme, "://", host})).host();
}

}  // namespace

namespace mojo {

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRewriteAddHeadersPtr,
                     fuchsia::web::UrlRequestRewriteAddHeaders> {
  static url_rewrite::mojom::UrlRequestRewriteAddHeadersPtr Convert(
      const fuchsia::web::UrlRequestRewriteAddHeaders& input) {
    url_rewrite::mojom::UrlRequestRewriteAddHeadersPtr add_headers =
        url_rewrite::mojom::UrlRequestRewriteAddHeaders::New();
    if (input.has_headers()) {
      for (const auto& header : input.headers()) {
        std::string_view header_name = BytesAsString(header.name);
        std::string_view header_value = BytesAsString(header.value);
        url_rewrite::mojom::UrlHeaderPtr url_header =
            url_rewrite::mojom::UrlHeader::New(std::string(header_name),
                                               std::string(header_value));
        add_headers->headers.push_back(std::move(url_header));
      }
    }
    return add_headers;
  }
};

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr,
                     fuchsia::web::UrlRequestRewriteRemoveHeader> {
  static url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr Convert(
      const fuchsia::web::UrlRequestRewriteRemoveHeader& input) {
    url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr remove_header =
        url_rewrite::mojom::UrlRequestRewriteRemoveHeader::New();
    if (input.has_query_pattern())
      remove_header->query_pattern = std::make_optional(input.query_pattern());
    if (input.has_header_name()) {
      remove_header->header_name =
          std::string(BytesAsString(input.header_name()));
    }
    return remove_header;
  }
};

template <>
struct TypeConverter<
    url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPatternPtr,
    fuchsia::web::UrlRequestRewriteSubstituteQueryPattern> {
  static url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPatternPtr Convert(
      const fuchsia::web::UrlRequestRewriteSubstituteQueryPattern& input) {
    url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPatternPtr
        substitute_query_pattern =
            url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPattern::New();
    if (input.has_pattern())
      substitute_query_pattern->pattern = input.pattern();
    if (input.has_substitution())
      substitute_query_pattern->substitution = input.substitution();
    return substitute_query_pattern;
  }
};

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRewriteReplaceUrlPtr,
                     fuchsia::web::UrlRequestRewriteReplaceUrl> {
  static url_rewrite::mojom::UrlRequestRewriteReplaceUrlPtr Convert(
      const fuchsia::web::UrlRequestRewriteReplaceUrl& input) {
    url_rewrite::mojom::UrlRequestRewriteReplaceUrlPtr replace_url =
        url_rewrite::mojom::UrlRequestRewriteReplaceUrl::New();
    if (input.has_url_ends_with())
      replace_url->url_ends_with = input.url_ends_with();
    if (input.has_new_url())
      replace_url->new_url = GURL(input.new_url());
    return replace_url;
  }
};

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRewriteAppendToQueryPtr,
                     fuchsia::web::UrlRequestRewriteAppendToQuery> {
  static url_rewrite::mojom::UrlRequestRewriteAppendToQueryPtr Convert(
      const fuchsia::web::UrlRequestRewriteAppendToQuery& input) {
    url_rewrite::mojom::UrlRequestRewriteAppendToQueryPtr append_to_query =
        url_rewrite::mojom::UrlRequestRewriteAppendToQuery::New();
    if (input.has_query())
      append_to_query->query = input.query();
    return append_to_query;
  }
};

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestAccessPolicy,
                     fuchsia::web::UrlRequestAction> {
  static url_rewrite::mojom::UrlRequestAccessPolicy Convert(
      const fuchsia::web::UrlRequestAction& input) {
    switch (input) {
      case fuchsia::web::UrlRequestAction::ALLOW:
        return url_rewrite::mojom::UrlRequestAccessPolicy::kAllow;
      case fuchsia::web::UrlRequestAction::DENY:
        return url_rewrite::mojom::UrlRequestAccessPolicy::kDeny;
    }
  }
};

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestActionPtr,
                     fuchsia::web::UrlRequestRewrite> {
  static url_rewrite::mojom::UrlRequestActionPtr Convert(
      const fuchsia::web::UrlRequestRewrite& input) {
    switch (input.Which()) {
      case fuchsia::web::UrlRequestRewrite::Tag::kAddHeaders:
        return url_rewrite::mojom::UrlRequestAction::NewAddHeaders(
            mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteAddHeadersPtr>(
                input.add_headers()));
      case fuchsia::web::UrlRequestRewrite::Tag::kRemoveHeader:
        return url_rewrite::mojom::UrlRequestAction::NewRemoveHeader(
            mojo::ConvertTo<
                url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr>(
                input.remove_header()));
      case fuchsia::web::UrlRequestRewrite::Tag::kSubstituteQueryPattern:
        return url_rewrite::mojom::UrlRequestAction::NewSubstituteQueryPattern(
            mojo::ConvertTo<
                url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPatternPtr>(
                input.substitute_query_pattern()));
      case fuchsia::web::UrlRequestRewrite::Tag::kReplaceUrl:
        return url_rewrite::mojom::UrlRequestAction::NewReplaceUrl(
            mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteReplaceUrlPtr>(
                input.replace_url()));
      case fuchsia::web::UrlRequestRewrite::Tag::kAppendToQuery:
        return url_rewrite::mojom::UrlRequestAction::NewAppendToQuery(
            mojo::ConvertTo<
                url_rewrite::mojom::UrlRequestRewriteAppendToQueryPtr>(
                input.append_to_query()));
      default:
        return nullptr;
    }
  }
};

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRulePtr,
                     fuchsia::web::UrlRequestRewriteRule> {
  static url_rewrite::mojom::UrlRequestRulePtr Convert(
      const fuchsia::web::UrlRequestRewriteRule& input) {
    url_rewrite::mojom::UrlRequestRulePtr rule =
        url_rewrite::mojom::UrlRequestRule::New();

    if (input.has_hosts_filter()) {
      // Convert host names in case they contain non-ASCII characters.
      const std::string_view kWildcard("*.");

      std::vector<std::string> hosts;
      for (const std::string_view host : input.hosts_filter()) {
        if (base::StartsWith(host, kWildcard, base::CompareCase::SENSITIVE)) {
          hosts.push_back(
              base::StrCat({kWildcard, NormalizeHost(host.substr(2))}));
        } else {
          hosts.push_back(NormalizeHost(host));
        }
      }
      rule->hosts_filter = std::move(hosts);
    }

    if (input.has_schemes_filter())
      rule->schemes_filter = std::make_optional(input.schemes_filter());

    if (input.has_rewrites()) {
      rule->actions =
          mojo::ConvertTo<std::vector<url_rewrite::mojom::UrlRequestActionPtr>>(
              input.rewrites());
    } else if (input.has_action()) {
      rule->actions = std::vector<url_rewrite::mojom::UrlRequestActionPtr>();
      rule->actions.push_back(url_rewrite::mojom::UrlRequestAction::NewPolicy(
          mojo::ConvertTo<url_rewrite::mojom::UrlRequestAccessPolicy>(
              input.action())));
    }

    return rule;
  }
};

url_rewrite::mojom::UrlRequestRewriteRulesPtr
TypeConverter<url_rewrite::mojom::UrlRequestRewriteRulesPtr,
              std::vector<fuchsia::web::UrlRequestRewriteRule>>::
    Convert(const std::vector<fuchsia::web::UrlRequestRewriteRule>& input) {
  url_rewrite::mojom::UrlRequestRewriteRulesPtr rules =
      url_rewrite::mojom::UrlRequestRewriteRules::New();
  for (const auto& rule : input) {
    rules->rules.push_back(
        mojo::ConvertTo<url_rewrite::mojom::UrlRequestRulePtr>(rule));
  }
  return rules;
}

}  // namespace mojo
