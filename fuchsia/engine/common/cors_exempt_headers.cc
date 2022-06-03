// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/common/cors_exempt_headers.h"

#include "base/containers/flat_set.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"

namespace {

base::LazyInstance<base::flat_set<std::string>>::Leaky
    g_cors_exempt_headers_lowercase = LAZY_INSTANCE_INITIALIZER;
}

void SetCorsExemptHeaders(const std::vector<std::string>& headers) {
  // Ensure that |g_cors_exempt_headers_lowercase| is created by accessing it.
  base::flat_set<std::string>* cors_exempt_headers =
      g_cors_exempt_headers_lowercase.Pointer();

  for (const std::string& header : headers)
    cors_exempt_headers->insert(base::ToLowerASCII(header));
}

bool IsHeaderCorsExempt(base::StringPiece header_name) {
  DCHECK(g_cors_exempt_headers_lowercase.IsCreated());

  return g_cors_exempt_headers_lowercase.Get().find(base::ToLowerASCII(
             header_name)) != g_cors_exempt_headers_lowercase.Get().end();
}
