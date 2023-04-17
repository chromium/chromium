// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/url_pattern.h"

namespace blink {

UrlPattern::UrlPattern() = default;

UrlPattern::~UrlPattern() = default;

bool operator==(const UrlPattern& left, const UrlPattern& right) {
  return left.pathname == right.pathname;
}

}  // namespace blink
