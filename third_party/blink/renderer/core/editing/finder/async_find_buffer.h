// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_ASYNC_FIND_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_ASYNC_FIND_BUFFER_H_

#include "base/callback.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"

namespace blink {
class AsyncFindBuffer {
 public:
  using Callback = base::OnceCallback<void(const EphemeralRangeInFlatTree&)>;
  static void FindMatchInRange(const EphemeralRangeInFlatTree& search_range,
                               String search_text,
                               FindOptions options,
                               Callback completeCallback);
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_BUFFER_H_
