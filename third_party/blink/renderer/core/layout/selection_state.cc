// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/selection_state.h"

#include <ostream>

#include "base/notreached.h"

namespace blink {

std::ostream& operator<<(std::ostream& out, const SelectionState state) {
  switch (state) {
    case SelectionState::kNone:
      return out << "None";
    case SelectionState::kStart:
      return out << "Start";
    case SelectionState::kInside:
      return out << "Inside";
    case SelectionState::kEnd:
      return out << "End";
    case SelectionState::kStartAndEnd:
      return out << "StartAndEnd";
    case SelectionState::kContain:
      return out << "Contain";
  }
  NOTREACHED_IN_MIGRATION();
  return out;
}

}  // namespace blink
