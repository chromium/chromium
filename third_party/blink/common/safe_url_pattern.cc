// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/safe_url_pattern.h"

namespace blink {

SafeUrlPattern::SafeUrlPattern() = default;

SafeUrlPattern::~SafeUrlPattern() = default;

bool operator==(const SafeUrlPattern& left, const SafeUrlPattern& right) {
  return left.pathname == right.pathname;
}

}  // namespace blink
