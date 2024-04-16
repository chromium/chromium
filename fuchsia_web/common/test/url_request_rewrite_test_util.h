// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_URL_REQUEST_REWRITE_TEST_UTIL_H_
#define FUCHSIA_WEB_COMMON_TEST_URL_REQUEST_REWRITE_TEST_UTIL_H_

#include <fuchsia/web/cpp/fidl.h>

#include <optional>
#include <string_view>

// Utility functions to create a fuchsia.web.UrlRequestRewrite in one line.

fuchsia::web::UrlRequestRewrite CreateRewriteAddHeaders(
    std::string_view header_name,
    std::string_view header_value);

fuchsia::web::UrlRequestRewrite CreateRewriteRemoveHeader(
    std::optional<std::string_view> query_pattern,
    std::string_view header_name);

fuchsia::web::UrlRequestRewrite CreateRewriteSubstituteQueryPattern(
    std::string_view pattern,
    std::string_view substitution);

fuchsia::web::UrlRequestRewrite CreateRewriteReplaceUrl(
    std::string_view url_ends_with,
    std::string_view new_url);

fuchsia::web::UrlRequestRewrite CreateRewriteAppendToQuery(
    std::string_view query);

#endif  // FUCHSIA_WEB_COMMON_TEST_URL_REQUEST_REWRITE_TEST_UTIL_H_
