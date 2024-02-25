// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_node_data.h"

#include "third_party/blink/renderer/core/layout/svg/svg_inline_node_data.h"

namespace blink {

void InlineNodeData::Trace(Visitor* visitor) const {
  visitor->Trace(first_line_items_);
  visitor->Trace(svg_node_data_);
  InlineItemsData::Trace(visitor);
}

}  // namespace blink
