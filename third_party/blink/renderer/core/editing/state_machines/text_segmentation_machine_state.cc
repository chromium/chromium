// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/text_segmentation_machine_state.h"

#include <array>
#include <ostream>

#include "base/check_op.h"

namespace blink {

std::ostream& operator<<(std::ostream& os, TextSegmentationMachineState state) {
  static const auto kTexts = std::to_array<const char*>({
      "Invalid",
      "NeedMoreCodeUnit",
      "NeedFollowingCodeUnit",
      "Finished",
  });
  DCHECK_LT(static_cast<size_t>(state), kTexts.size()) << "Unknown state value";
  return os << kTexts[static_cast<size_t>(state)];
}

}  // namespace blink
