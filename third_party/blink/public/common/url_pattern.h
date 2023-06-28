// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_URL_PATTERN_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_URL_PATTERN_H_

#include <vector>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/liburlpattern/pattern.h"

namespace blink {
struct BLINK_COMMON_EXPORT UrlPattern {
  UrlPattern();
  ~UrlPattern();

  std::vector<liburlpattern::Part> pathname;
};

BLINK_COMMON_EXPORT bool operator==(const UrlPattern& left,
                                    const UrlPattern& right);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_URL_PATTERN_H_
