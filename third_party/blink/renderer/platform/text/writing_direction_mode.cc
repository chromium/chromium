// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"

#include <ostream>

namespace blink {

std::ostream& operator<<(std::ostream& ostream,
                         const WritingDirectionMode& writing_direction) {
  return ostream << writing_direction.GetWritingMode() << " "
                 << writing_direction.Direction();
}

}  // namespace blink
