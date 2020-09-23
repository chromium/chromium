// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"

#include <ostream>

namespace blink {

std::ostream& operator<<(std::ostream& os, PaintChunkIterator it) {
  return os << it.ToString();
}

}  // namespace blink
