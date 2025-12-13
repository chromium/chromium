// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"

#include <utility>

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_visualizer.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"

namespace blink {

PaintTimingRecord::PaintTimingRecord(Node* node,
                                     uint64_t recorded_size,
                                     const gfx::Rect& frame_visual_rect,
                                     const gfx::RectF& root_visual_rect,
                                     SoftNavigationContext* context)
    : node_(node),
      recorded_size_(recorded_size),
      root_visual_rect_(root_visual_rect),
      soft_navigation_context_(context),
      lcp_rect_info_(PaintTimingVisualizer::IsTracingEnabled()
                         ? std::make_unique<LCPRectInfo>(
                               frame_visual_rect,
                               gfx::ToRoundedRect(root_visual_rect))
                         : nullptr) {
  CHECK(node_);
}

void PaintTimingRecord::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  visitor->Trace(soft_navigation_context_);
}

int PaintTimingRecord::NodeIdForTracing() const {
  return node_ ? static_cast<int>(node_->GetDomNodeId()) : 0;
}

void PaintTimingRecord::PopulateTraceValue(TracedValue& value) const {
  value.SetString("nodeName", node_ ? node_->DebugName() : "(null)");
  value.SetInteger("DOMNodeId", NodeIdForTracing());
  value.SetInteger("size", static_cast<int>(RecordedSize()));
  if (lcp_rect_info_) {
    lcp_rect_info_->OutputToTraceValue(value);
  }
}

TextRecord::TextRecord(Node* node,
                       uint64_t new_recorded_size,
                       const gfx::RectF& element_timing_rect,
                       const gfx::Rect& frame_visual_rect,
                       const gfx::RectF& root_visual_rect,
                       bool is_needed_for_element_timing,
                       SoftNavigationContext* soft_navigation_context)
    : PaintTimingRecord(node,
                        new_recorded_size,
                        frame_visual_rect,
                        root_visual_rect,
                        soft_navigation_context),
      element_timing_rect_(element_timing_rect),
      is_needed_for_element_timing_(is_needed_for_element_timing) {}

ImageRecord::ImageRecord(Node* node,
                         const MediaTiming* new_media_timing,
                         uint64_t new_recorded_size,
                         const gfx::Rect& frame_visual_rect,
                         const gfx::RectF& root_visual_rect,
                         MediaRecordIdHash hash,
                         double entropy_for_lcp,
                         SoftNavigationContext* soft_navigation_context)
    : PaintTimingRecord(node,
                        new_recorded_size,
                        frame_visual_rect,
                        root_visual_rect,
                        soft_navigation_context),
      media_timing_(new_media_timing),
      hash_(hash),
      entropy_for_lcp_(entropy_for_lcp) {
  CHECK_GT(RecordedSize(), 0u);
}

std::optional<WebURLRequest::Priority> ImageRecord::RequestPriority() const {
  if (!GetMediaTiming()) {
    return std::nullopt;
  }
  return GetMediaTiming()->RequestPriority();
}

void ImageRecord::Trace(Visitor* visitor) const {
  visitor->Trace(media_timing_);
  PaintTimingRecord::Trace(visitor);
}

void ImageRecord::PopulateTraceValue(TracedValue& value) const {
  PaintTimingRecord::PopulateTraceValue(value);

  // The media_timing could have been deleted when this is called.
  value.SetString("imageUrl", GetMediaTiming() ? String(GetMediaTiming()->Url())
                                               : "(deleted)");
}

}  // namespace blink
