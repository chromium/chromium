// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_start_targets.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

class LayoutBox;

void ScrollStartTargetCandidates::Trace(Visitor* visitor) const {
  visitor->Trace(y);
  visitor->Trace(x);
}

}  // namespace blink
