// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LCP_OBJECTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LCP_OBJECTS_H_

#include <optional>

#include "base/time/time.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

struct ResourceLoadTimings {
  base::TimeTicks load_start;
  base::TimeTicks load_end;
  base::TimeTicks discovery_time;
};

struct LargestImagePaintDetails {
  base::TimeTicks presentation_time;
  uint64_t paint_size = 0;
  ResourceLoadTimings resource_load_timings = {};
  blink::LargestContentfulPaintType type =
      blink::LargestContentfulPaintType::kNone;
  double bpp = 0.0;
  std::optional<WebURLRequest::Priority> request_priority = std::nullopt;
};

struct LargestTextPaintDetails {
  base::TimeTicks presentation_time;
  uint64_t paint_size = 0;
};

struct LargestContentfulPaintDetails {
  // Returns the type of the LCP candidate, selecting between `largest_text` and
  // `largest_image`.
  blink::LargestContentfulPaintType Type() const {
    return IsCandidateText() ? blink::LargestContentfulPaintType::kText
                             : largest_image.type;
  }

  // Returns the presentation time of the LCP candidate, selecting between
  // `largest_text` and `largest_image`.
  base::TimeTicks PresentationTime() const {
    return IsCandidateText() ? largest_text.presentation_time
                             : largest_image.presentation_time;
  }

  LargestImagePaintDetails largest_image;
  LargestTextPaintDetails largest_text;

 private:
  // Returns true iff the `largest_text` is the LCP candidate.
  bool IsCandidateText() const {
    return largest_text.paint_size > largest_image.paint_size ||
           (largest_text.paint_size == largest_image.paint_size &&
            largest_text.presentation_time < largest_image.presentation_time);
  }
};

// This class is used for tracing only.
class LCPRectInfo {
  USING_FAST_MALLOC(LCPRectInfo);

 public:
  LCPRectInfo(const gfx::Rect& frame_rect_info, const gfx::Rect& root_rect_info)
      : frame_rect_info_(frame_rect_info), root_rect_info_(root_rect_info) {}

  void OutputToTraceValue(TracedValue&) const;

 private:
  gfx::Rect frame_rect_info_;
  gfx::Rect root_rect_info_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LCP_OBJECTS_H_
