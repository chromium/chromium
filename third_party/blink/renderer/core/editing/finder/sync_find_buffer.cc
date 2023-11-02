// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/editing/finder/sync_find_buffer.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"

namespace blink {

void SyncFindBuffer::FindMatchInRange(RangeInFlatTree* search_range,
                                      String search_text,
                                      FindOptions options,
                                      Callback completeCallback) {
  EphemeralRangeInFlatTree range = FindBuffer::FindMatchInRange(
      search_range->ToEphemeralRange(), search_text, options);

  DCHECK(range.IsNull() || !range.IsCollapsed());

  // Search finished, return the result
  std::move(completeCallback).Run(range);
}

}  // namespace blink
