// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/min_max_sizes.h"

#include <algorithm>

namespace blink {

std::ostream& operator<<(std::ostream& stream, const MinMaxSizes& value) {
  return stream << "(" << value.min_size << ", " << value.max_size << ")";
}

}  // namespace blink
