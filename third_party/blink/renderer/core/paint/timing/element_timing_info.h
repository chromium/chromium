// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_INFO_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class Element;

// `ElementTimingInfo` contains information captured during paint time needed to
// create element timing or container timing entries at presentation time.
//
// TODO(crbug.com/535432431): Use ImageRecord directly and delete this class.
struct ElementTimingInfo final : public GarbageCollected<ElementTimingInfo> {
  ElementTimingInfo(const String& url,
                    const gfx::RectF& rect,
                    const base::TimeTicks& response_end,
                    const AtomicString& identifier,
                    const gfx::Size& intrinsic_size,
                    const AtomicString& id,
                    Element* element);

  ElementTimingInfo(const ElementTimingInfo&) = delete;
  ElementTimingInfo& operator=(const ElementTimingInfo&) = delete;

  void Trace(Visitor* visitor) const;

  const String url;
  const gfx::RectF rect;
  const base::TimeTicks response_end;
  const AtomicString identifier;
  const gfx::Size intrinsic_size;
  const AtomicString id;
  const Member<Element> element;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_INFO_H_
