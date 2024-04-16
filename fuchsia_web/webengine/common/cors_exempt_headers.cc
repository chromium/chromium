// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/common/cors_exempt_headers.h"

#include <string_view>

#include "base/containers/contains.h"
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

bool IsHeaderCorsExempt(std::string_view header_name) {
  DCHECK(g_cors_exempt_headers_lowercase.IsCreated());

  const auto& cors_exempt_headers_set = g_cors_exempt_headers_lowercase.Get();
  return base::Contains(cors_exempt_headers_set,
                        base::ToLowerASCII(header_name));
}
