// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_LIST_MARKER_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"

namespace blink {

class LineLayoutListMarker : public LineLayoutItem {
 public:
  explicit LineLayoutListMarker(LayoutObject* object) : LineLayoutItem(object) {
    SECURITY_DCHECK(!object || object->IsListMarker());
  }

  explicit LineLayoutListMarker(const LineLayoutItem& item)
      : LineLayoutItem(item) {
    SECURITY_DCHECK(!item || item.IsListMarker());
  }

  explicit LineLayoutListMarker(std::nullptr_t) : LineLayoutItem(nullptr) {}

  LineLayoutListMarker() = default;

  bool IsInside() const { return GetLayoutObject()->IsInsideListMarker(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_LIST_MARKER_H_
