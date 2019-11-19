// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_API_SHIM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_API_SHIM_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;

// TODO(pilgrim): Remove this kludge once clients have a real API and no longer
// need access to the underlying LayoutObject.
class LineLayoutAPIShim {
  STATIC_ONLY(LineLayoutAPIShim);

 public:
  static LayoutObject* LayoutObjectFrom(LineLayoutItem item) {
    return item.GetLayoutObject();
  }

  static const LayoutObject* ConstLayoutObjectFrom(LineLayoutItem item) {
    return item.GetLayoutObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_API_SHIM_H_
