// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/url_request_rewrite_type_converters.h"

#include "base/strings/strcat.h"

namespace {

std::string NormalizeHost(base::StringPiece host) {
  return GURL(base::StrCat({url::kHttpScheme, "://", host})).host();
}

}  // namespace

namespace mojo {

mojom::UrlRequestRewriteAddHeadersPtr
TypeConverter<mojom::UrlRequestRewriteAddHeadersPtr,
              fuchsia::web::UrlRequestRewriteAddHeaders>::
    Convert(const fuchsia::web::UrlRequestRewriteAddHeaders& input) {
  mojom::UrlRequestRewriteAddHeadersPtr add_headers =
      mojom::UrlRequestRewriteAddHeaders::New();
  if (input.has_headers()) {
    for (const auto& header : input.headers()) {
      base::StringPiece header_name = cr_fuchsia::BytesAsString(header.name);
      base::StringPiece header_value = cr_fuchsia::BytesAsString(header.value);
      add_headers->headers.SetHeader(header_name, header_value);
    }
  }
  return add_headers;
}

mojom::UrlRequestRewriteRemoveHeaderPtr
TypeConverter<mojom::UrlRequestRewriteRemoveHeaderPtr,
              fuchsia::web::UrlRequestRewriteRemoveHeader>::
    Convert(const fuchsia::web::UrlRequestRewriteRemoveHeader& input) {
  mojom::UrlRequestRewriteRemoveHeaderPtr remove_header =
      mojom::UrlRequestRewriteRemoveHeader::New();
  if (input.has_query_pattern())
    remove_header->query_pattern = base::make_optional(input.query_pattern());
  if (input.has_header_name())
    remove_header->header_name =
        cr_fuchsia::BytesAsString(input.header_name()).as_string();
  return remove_header;
}

mojom::UrlRequestRewriteSubstituteQueryPatternPtr
TypeConverter<mojom::UrlRequestRewriteSubstituteQueryPatternPtr,
              fuchsia::web::UrlRequestRewriteSubstituteQueryPattern>::
    Convert(
        const fuchsia::web::UrlRequestRewriteSubstituteQueryPattern& input) {
  mojom::UrlRequestRewriteSubstituteQueryPatternPtr substitute_query_pattern =
      mojom::UrlRequestRewriteSubstituteQueryPattern::New();
  if (input.has_pattern())
    substitute_query_pattern->pattern = input.pattern();
  if (input.has_substitution())
    substitute_query_pattern->substitution = input.substitution();
  return substitute_query_pattern;
}

mojom::UrlRequestRewriteReplaceUrlPtr
TypeConverter<mojom::UrlRequestRewriteReplaceUrlPtr,
              fuchsia::web::UrlRequestRewriteReplaceUrl>::
    Convert(const fuchsia::web::UrlRequestRewriteReplaceUrl& input) {
  mojom::UrlRequestRewriteReplaceUrlPtr replace_url =
      mojom::UrlRequestRewriteReplaceUrl::New();
  if (input.has_url_ends_with())
    replace_url->url_ends_with = input.url_ends_with();
  if (input.has_new_url())
    replace_url->new_url = GURL(input.new_url());
  return replace_url;
}

mojom::UrlRequestRewritePtr
TypeConverter<mojom::UrlRequestRewritePtr, fuchsia::web::UrlRequestRewrite>::
    Convert(const fuchsia::web::UrlRequestRewrite& input) {
  switch (input.Which()) {
    case fuchsia::web::UrlRequestRewrite::Tag::kAddHeaders:
      return mojom::UrlRequestRewrite::NewAddHeaders(
          mojo::ConvertTo<mojom::UrlRequestRewriteAddHeadersPtr>(
              input.add_headers()));
    case fuchsia::web::UrlRequestRewrite::Tag::kRemoveHeader:
      return mojom::UrlRequestRewrite::NewRemoveHeader(
          mojo::ConvertTo<mojom::UrlRequestRewriteRemoveHeaderPtr>(
              input.remove_header()));
    case fuchsia::web::UrlRequestRewrite::Tag::kSubstituteQueryPattern:
      return mojom::UrlRequestRewrite::NewSubstituteQueryPattern(
          mojo::ConvertTo<mojom::UrlRequestRewriteSubstituteQueryPatternPtr>(
              input.substitute_query_pattern()));
    case fuchsia::web::UrlRequestRewrite::Tag::kReplaceUrl:
      return mojom::UrlRequestRewrite::NewReplaceUrl(
          mojo::ConvertTo<mojom::UrlRequestRewriteReplaceUrlPtr>(
              input.replace_url()));
    default:
      // This is to prevent build breakage when adding new rewrites to the FIDL
      // definition.
      NOTREACHED();
      return nullptr;
  }
}

mojom::UrlRequestRewriteRulePtr
TypeConverter<mojom::UrlRequestRewriteRulePtr,
              fuchsia::web::UrlRequestRewriteRule>::
    Convert(const fuchsia::web::UrlRequestRewriteRule& input) {
  mojom::UrlRequestRewriteRulePtr rule = mojom::UrlRequestRewriteRule::New();
  if (input.has_hosts_filter()) {
    // Convert host names in case they contain non-ASCII characters.
    const base::StringPiece kWildcard("*.");

    std::vector<std::string> hosts;
    for (const base::StringPiece host : input.hosts_filter()) {
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
    rule->schemes_filter = base::make_optional(input.schemes_filter());
  if (input.has_rewrites())
    rule->rewrites = mojo::ConvertTo<std::vector<mojom::UrlRequestRewritePtr>>(
        input.rewrites());
  return rule;
}

}  // namespace mojo
