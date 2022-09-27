// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/api/line_layout_text.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

LayoutTextSelectionStatus LineLayoutText::SelectionStatus() const {
  return ToText()->GetFrame()->Selection().ComputeLayoutSelectionStatus(
      *ToText());
}

}  // namespace blink
