// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SAFE_URL_PATTERN_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SAFE_URL_PATTERN_H_

#include <vector>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/liburlpattern/pattern.h"

// SafeUrlPattern is a reduced version of URLPattern.
// It is used in both the browser process and the renderer process, and it does
// not allow a user-defined regular expression due to a security concern.
namespace blink {
struct BLINK_COMMON_EXPORT SafeUrlPattern {
  SafeUrlPattern();
  ~SafeUrlPattern();

  std::vector<liburlpattern::Part> hostname;
  std::vector<liburlpattern::Part> pathname;
};

BLINK_COMMON_EXPORT bool operator==(const SafeUrlPattern& left,
                                    const SafeUrlPattern& right);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SAFE_URL_PATTERN_H_
