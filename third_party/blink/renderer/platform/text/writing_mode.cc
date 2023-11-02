// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/writing_mode.h"

#include <ostream>

namespace blink {

std::ostream& operator<<(std::ostream& ostream, WritingMode writing_mode) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return ostream << "horizontal-tb";
    case WritingMode::kVerticalRl:
      return ostream << "vertical-rl";
    case WritingMode::kVerticalLr:
      return ostream << "vertical-lr";
    case WritingMode::kSidewaysRl:
      return ostream << "sideways-rl";
    case WritingMode::kSidewaysLr:
      return ostream << "sideways-lr";
  }
  return ostream << static_cast<unsigned>(writing_mode);
}

}  // namespace blink
