// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_later_result.h"

namespace blink {

FetchLaterResult::FetchLaterResult() = default;

void FetchLaterResult::SetActivated(bool activated) {
  activated_ = activated;
}

bool FetchLaterResult::activated() const {
  return activated_;
}

}  // namespace blink
