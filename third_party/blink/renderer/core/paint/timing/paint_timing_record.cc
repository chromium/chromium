// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"

#include <utility>

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_visualizer.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"

namespace blink {

PaintTimingRecord::PaintTimingRecord(Node* node,
                                     const gfx::Rect& frame_visual_rect,
                                     const gfx::RectF& root_visual_rect)
    : node_(node),
      layout_object_(node->GetLayoutObject()),
      root_visual_rect_(root_visual_rect),
      lcp_rect_info_(PaintTimingVisualizer::IsTracingEnabled()
                         ? std::make_optional<LCPRectInfo>(
                               frame_visual_rect,
                               gfx::ToRoundedRect(root_visual_rect))
                         : std::nullopt) {
  CHECK(node_);
}

void PaintTimingRecord::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  visitor->Trace(layout_object_);
  visitor->Trace(soft_navigation_context_);
}

int PaintTimingRecord::NodeIdForTracing() const {
  return node_ ? static_cast<int>(node_->GetDomNodeId()) : 0;
}

void PaintTimingRecord::PopulateTraceValue(TracedValue& value) const {
  value.SetString("nodeName", node_ ? node_->DebugName() : "(null)");
  value.SetInteger("DOMNodeId", NodeIdForTracing());
  value.SetInteger("size", static_cast<int>(EffectiveVisualSize()));
  if (lcp_rect_info_) {
    lcp_rect_info_->OutputToTraceValue(value);
  }
}

bool PaintTimingRecord::WasNodeRemoved() const {
  return !node_ || !node_->GetLayoutObject() ||
         node_->GetLayoutObject() != layout_object_;
}

TextRecord::TextRecord(Node* node,
                       uint64_t effective_visual_size,
                       const gfx::RectF& element_timing_rect,
                       const gfx::Rect& frame_visual_rect,
                       const gfx::RectF& root_visual_rect)
    : PaintTimingRecord(node, frame_visual_rect, root_visual_rect),
      effective_visual_size_(effective_visual_size),
      element_timing_rect_(element_timing_rect) {}

ImageRecord::ImageRecord(
    Node* node,
    const MediaTiming* new_media_timing,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect,
    MediaRecordIdHash hash,
    const EffectiveVisualSizeResult& effective_visual_size_result)
    : PaintTimingRecord(node, frame_visual_rect, root_visual_rect),
      media_timing_(new_media_timing),
      hash_(hash),
      effective_visual_size_result_(effective_visual_size_result) {
  CHECK_GT(EffectiveVisualSize(), 0u);
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
