// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/url_request_rewrite_test_util.h"

#include "fuchsia/base/string_util.h"

namespace cr_fuchsia {

fuchsia::web::UrlRequestRewrite CreateRewriteAddHeaders(
    base::StringPiece header_name,
    base::StringPiece header_value) {
  fuchsia::net::http::Header header;
  header.name = cr_fuchsia::StringToBytes(header_name);
  header.value = cr_fuchsia::StringToBytes(header_value);

  std::vector<fuchsia::net::http::Header> response_headers;
  response_headers.push_back(std::move(header));

  fuchsia::web::UrlRequestRewriteAddHeaders add_headers;
  add_headers.set_headers(std::move(response_headers));
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_add_headers(std::move(add_headers));

  return rewrite;
}

fuchsia::web::UrlRequestRewrite CreateRewriteRemoveHeader(
    base::Optional<base::StringPiece> query_pattern,
    base::StringPiece header_name) {
  fuchsia::web::UrlRequestRewriteRemoveHeader remove_header;
  if (query_pattern)
    remove_header.set_query_pattern(query_pattern.value().as_string());
  remove_header.set_header_name(cr_fuchsia::StringToBytes(header_name));
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_remove_header(std::move(remove_header));

  return rewrite;
}

fuchsia::web::UrlRequestRewrite CreateRewriteSubstituteQueryPattern(
    base::StringPiece pattern,
    base::StringPiece substitution) {
  fuchsia::web::UrlRequestRewriteSubstituteQueryPattern
      substitute_query_pattern;
  substitute_query_pattern.set_pattern(pattern.as_string());
  substitute_query_pattern.set_substitution(substitution.as_string());
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_substitute_query_pattern(std::move(substitute_query_pattern));

  return rewrite;
}

fuchsia::web::UrlRequestRewrite CreateRewriteReplaceUrl(
    base::StringPiece url_ends_with,
    base::StringPiece new_url) {
  fuchsia::web::UrlRequestRewriteReplaceUrl replace_url;
  replace_url.set_url_ends_with(url_ends_with.as_string());
  replace_url.set_new_url(new_url.as_string());
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_replace_url(std::move(replace_url));

  return rewrite;
}

}  // namespace cr_fuchsia
