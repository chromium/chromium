// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_later_result.h"

namespace blink {

FetchLaterResult::FetchLaterResult(bool sent) : sent_(sent) {}

bool FetchLaterResult::sent() const {
  // TODO(crbug.com/1465781): Dynamically ask FetchManager to decide the state.
  return sent_;
}

}  // namespace blink
