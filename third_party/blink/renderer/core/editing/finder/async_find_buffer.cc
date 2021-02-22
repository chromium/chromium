// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/editing/finder/async_find_buffer.h"

namespace blink {

void AsyncFindBuffer::FindMatchInRange(
    const EphemeralRangeInFlatTree& search_range,
    String search_text,
    FindOptions options,
    Callback completeCallback) {
  EphemeralRangeInFlatTree range =
      FindBuffer::FindMatchInRange(search_range, search_text, options);
  // TODO(gayane): Handle the case |FindBuffer::FindMatchInRange| didn't
  // finished the search within specified timeout.
  DCHECK(range.IsNull() || !range.IsCollapsed());

  // Search finished, return the result
  std::move(completeCallback).Run(range);
}
}  // namespace blink
