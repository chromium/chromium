// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CALLBACKS_H_

#include <optional>

#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace base {
class TimeTicks;
}

namespace blink {
struct DOMPaintTimingInfo;

// Presentation time callback used for PaintTiming clients.
using PaintTimingCallback =
    base::OnceCallback<void(const base::TimeTicks&, const DOMPaintTimingInfo&)>;
using OptionalPaintTimingCallback = std::optional<PaintTimingCallback>;

// Presentation time callback for the image and text paint timing detectors. The
// detectors should update the presentation time for any pending
// `PaintTimingRecord`s for the relevant frame and add them to the given vector.
template <IsDerivedFromPaintTimingRecord T>
using PaintTimingDetectorCallback =
    base::OnceCallback<void(const base::TimeTicks&,
                            const DOMPaintTimingInfo&,
                            HeapVector<Member<T>>&)>;
template <IsDerivedFromPaintTimingRecord T>
using OptionalPaintTimingDetectorCallback =
    std::optional<PaintTimingDetectorCallback<T>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CALLBACKS_H_
