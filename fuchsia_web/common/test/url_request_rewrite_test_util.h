// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_URL_REQUEST_REWRITE_TEST_UTIL_H_
#define FUCHSIA_WEB_COMMON_TEST_URL_REQUEST_REWRITE_TEST_UTIL_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Utility functions to create a fuchsia.web.UrlRequestRewrite in one line.

fuchsia::web::UrlRequestRewrite CreateRewriteAddHeaders(
    base::StringPiece header_name,
    base::StringPiece header_value);

fuchsia::web::UrlRequestRewrite CreateRewriteRemoveHeader(
    absl::optional<base::StringPiece> query_pattern,
    base::StringPiece header_name);

fuchsia::web::UrlRequestRewrite CreateRewriteSubstituteQueryPattern(
    base::StringPiece pattern,
    base::StringPiece substitution);

fuchsia::web::UrlRequestRewrite CreateRewriteReplaceUrl(
    base::StringPiece url_ends_with,
    base::StringPiece new_url);

fuchsia::web::UrlRequestRewrite CreateRewriteAppendToQuery(
    base::StringPiece query);

#endif  // FUCHSIA_WEB_COMMON_TEST_URL_REQUEST_REWRITE_TEST_UTIL_H_
