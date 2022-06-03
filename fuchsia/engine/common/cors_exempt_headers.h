// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_COMMON_CORS_EXEMPT_HEADERS_H_
#define FUCHSIA_ENGINE_COMMON_CORS_EXEMPT_HEADERS_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "fuchsia/engine/web_engine_export.h"

// Sets the list of HTTP header names which will bypass CORS enforcement when
// injected.
WEB_ENGINE_EXPORT void SetCorsExemptHeaders(
    const std::vector<std::string>& headers);

// Returns true if the header with |header_name| may bypass CORS when injected.
// Matching of |header_name| is case insensitive, as Chromium's net internals
// don't normalize the casing of header names.
// May only be called after SetCorsExemptHeaders() is invoked.
WEB_ENGINE_EXPORT bool IsHeaderCorsExempt(base::StringPiece header_name);

#endif  // FUCHSIA_ENGINE_COMMON_CORS_EXEMPT_HEADERS_H_
