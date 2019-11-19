// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"

namespace blink {

std::ostream& operator<<(std::ostream& stream, const LogicalSize& value) {
  return stream << value.inline_size << "x" << value.block_size;
}

}  // namespace blink
