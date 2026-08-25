// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/element_timing_info.h"

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

ElementTimingInfo::ElementTimingInfo(const String& url,
                                     const gfx::RectF& rect,
                                     const base::TimeTicks& response_end,
                                     const AtomicString& identifier,
                                     const gfx::Size& intrinsic_size,
                                     const AtomicString& id,
                                     Element* element)
    : url(url),
      rect(rect),
      response_end(response_end),
      identifier(identifier),
      intrinsic_size(intrinsic_size),
      id(id),
      element(element) {}

void ElementTimingInfo::Trace(Visitor* visitor) const {
  visitor->Trace(element);
}

}  // namespace blink
