// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_servable_state.h"

namespace content {

CONTENT_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                        PrefetchServableState servable_state) {
  switch (servable_state) {
    case PrefetchServableState::kNotServable:
      return ostream << "NotServable";
    case PrefetchServableState::kServable:
      return ostream << "Servable";
    case PrefetchServableState::kShouldBlockUntilHeadReceived:
      return ostream << "ShouldBlockUntilHeadReceived";
    case PrefetchServableState::kShouldBlockUntilEligibilityGot:
      return ostream << "ShouldBlockUntilEligibilityGot";
  }
}

}  // namespace content
