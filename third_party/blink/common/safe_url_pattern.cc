// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/safe_url_pattern.h"

#include <algorithm>
#include <iterator>
#include <ostream>
#include <string_view>
#include <tuple>
#include <vector>

#include "third_party/liburlpattern/part.h"

namespace blink {

SafeUrlPattern::SafeUrlPattern() = default;

SafeUrlPattern::SafeUrlPattern(const SafeUrlPattern&) = default;

SafeUrlPattern& SafeUrlPattern::operator=(const SafeUrlPattern&) = default;

SafeUrlPattern::SafeUrlPattern(SafeUrlPattern&&) = default;

SafeUrlPattern& SafeUrlPattern::operator=(SafeUrlPattern&&) = default;

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

void PrintTo(const SafeUrlPattern& pattern, std::ostream* o) {
  auto print_parts = [o](std::string_view label,
                         const std::vector<liburlpattern::Part>& field) {
    *o << "\n  " << label << ": [ ";
    std::ranges::copy(field,
                      std::ostream_iterator<liburlpattern::Part>(*o, " "));
    *o << "]";
  };
  *o << "{";
  print_parts("protocol", pattern.protocol);
  print_parts("username", pattern.username);
  print_parts("password", pattern.password);
  print_parts("hostname", pattern.hostname);
  print_parts("port", pattern.port);
  print_parts("pathname", pattern.pathname);
  print_parts("search", pattern.search);
  print_parts("hash", pattern.hash);
  *o << "\n  options: { ignore_case: "
     << (pattern.options.ignore_case ? "true" : "false") << " }";
  *o << "\n}";
}

}  // namespace blink
