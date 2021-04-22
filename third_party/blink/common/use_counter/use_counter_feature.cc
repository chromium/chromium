// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/use_counter/use_counter_feature.h"

namespace blink {
bool operator==(const UseCounterFeature& lhs, const UseCounterFeature& rhs) {
  return std::tie(lhs.type, lhs.value) == std::tie(rhs.type, rhs.value);
}

bool operator<(const UseCounterFeature& lhs, const UseCounterFeature& rhs) {
  return std::tie(lhs.type, lhs.value) < std::tie(rhs.type, rhs.value);
}

}  // namespace blink
