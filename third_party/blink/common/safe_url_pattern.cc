// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/safe_url_pattern.h"

#include <tuple>

namespace blink {

SafeUrlPattern::SafeUrlPattern() = default;

SafeUrlPattern::~SafeUrlPattern() = default;

bool operator==(const SafeUrlPattern& left, const SafeUrlPattern& right) {
  auto fields = [](const SafeUrlPattern& p) {
    return std::tie(p.protocol, p.username, p.password, p.hostname, p.port,
                    p.pathname, p.search, p.hash, p.options);
  };
  return fields(left) == fields(right);
}

bool operator==(const SafeUrlPatternOptions& left,
                const SafeUrlPatternOptions& right) {
  auto fields = [](const SafeUrlPatternOptions& op) {
    return std::tie(op.ignore_case);
  };

  return fields(left) == fields(right);
}

}  // namespace blink
