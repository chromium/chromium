// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_INFO_H_

#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"

namespace blink {

// https://w3c.github.io/paint-timing/#paint-timing-info
struct PaintTimingInfo {
  // https://w3c.github.io/paint-timing/#paint-timing-info-rendering-update-end-time
  base::TimeTicks rendering_update_end_time;

  // https://w3c.github.io/paint-timing/#paint-timing-info-implementation-defined-presentation-time
  base::TimeTicks presentation_time;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_INFO_H_
