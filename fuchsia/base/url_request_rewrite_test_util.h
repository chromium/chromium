// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_URL_REQUEST_REWRITE_TEST_UTIL_H_
#define FUCHSIA_BASE_URL_REQUEST_REWRITE_TEST_UTIL_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/optional.h"
#include "base/strings/string_piece.h"

namespace cr_fuchsia {

// Utility functions to create a fuchsia.web.UrlRequestRewrite in one line.

fuchsia::web::UrlRequestRewrite CreateRewriteAddHeaders(
    base::StringPiece header_name,
    base::StringPiece header_value);

fuchsia::web::UrlRequestRewrite CreateRewriteRemoveHeader(
    base::Optional<base::StringPiece> query_pattern,
    base::StringPiece header_name);

fuchsia::web::UrlRequestRewrite CreateRewriteSubstituteQueryPattern(
    base::StringPiece pattern,
    base::StringPiece substitution);

fuchsia::web::UrlRequestRewrite CreateRewriteReplaceUrl(
    base::StringPiece url_ends_with,
    base::StringPiece new_url);

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_URL_REQUEST_REWRITE_TEST_UTIL_H_
