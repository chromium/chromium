// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CALLBACKS_H_

#include <optional>

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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CALLBACKS_H_
