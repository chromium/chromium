// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_timing_visualizer.h"

#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

namespace {
static void CreateQuad(TracedValue* value,
                       const char* name,
                       const FloatQuad& quad) {
  value->BeginArray(name);
  value->PushDouble(quad.P1().X());
  value->PushDouble(quad.P1().Y());
  value->PushDouble(quad.P2().X());
  value->PushDouble(quad.P2().Y());
  value->PushDouble(quad.P3().X());
  value->PushDouble(quad.P3().Y());
  value->PushDouble(quad.P4().X());
  value->PushDouble(quad.P4().Y());
  value->EndArray();
}

}  // namespace

void PaintTimingVisualizer::RecordRects(const IntRect& rect,
                                        std::unique_ptr<TracedValue>& value) {
  CreateQuad(value.get(), "rect", FloatRect(rect));
}
void PaintTimingVisualizer::RecordObject(const LayoutObject& object,
                                         std::unique_ptr<TracedValue>& value) {
  value->SetString("object_name", object.GetName());
  DCHECK(object.GetFrame());
  value->SetString("frame", String::FromUTF8(ToTraceValue(object.GetFrame())));
  value->SetBoolean("is_in_main_frame", object.GetFrame()->IsMainFrame());
  if (object.GetNode())
    value->SetInteger("dom_node_id", DOMNodeIds::IdForNode(object.GetNode()));
}

void PaintTimingVisualizer::DumpTextDebuggingRect(const LayoutObject& object,
                                                  const FloatRect& rect) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  RecordObject(object, value);
  RecordRects(RoundedIntRect(rect), value);
  value->SetBoolean("is_aggregation_text", true);
  value->SetBoolean("is_svg", object.IsSVG());
  DumpTrace(std::move(value));
}

void PaintTimingVisualizer::DumpImageDebuggingRect(
    const LayoutObject& object,
    const FloatRect& rect,
    const ImageResourceContent& cached_image) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  RecordObject(object, value);
  RecordRects(RoundedIntRect(rect), value);
  value->SetBoolean("is_image", true);
  value->SetBoolean("is_svg", object.IsSVG());
  value->SetBoolean("is_image_loaded", cached_image.IsLoaded());
  value->SetString("image_url",
                   String(cached_image.Url().StrippedForUseAsReferrer()));
  DumpTrace(std::move(value));
}

void PaintTimingVisualizer::DumpTrace(std::unique_ptr<TracedValue> value) {
  TRACE_EVENT_INSTANT1("loading", "PaintTimingVisualizer::LayoutObjectPainted",
                       TRACE_EVENT_SCOPE_THREAD, "data", std::move(value));
}

void PaintTimingVisualizer::RecordMainFrameViewport(
    LocalFrameView& frame_view) {
  if (!need_recording_viewport)
    return;
  if (!frame_view.GetFrame().IsMainFrame())
    return;
  ScrollableArea* scrollable_area = frame_view.GetScrollableArea();
  DCHECK(scrollable_area);
  IntRect viewport_rect = scrollable_area->VisibleContentRect();

  FloatClipRect float_clip_visual_rect =
      FloatClipRect(FloatRect(viewport_rect));
  WebFloatRect float_visual_rect = float_clip_visual_rect.Rect();
  frame_view.GetPaintTimingDetector().ConvertViewportToWindow(
      &float_visual_rect);

  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  CreateQuad(value.get(), "viewport_rect", FloatQuad(float_visual_rect));
  TRACE_EVENT_INSTANT1("loading", "PaintTimingVisualizer::Viewport",
                       TRACE_EVENT_SCOPE_THREAD, "data", std::move(value));
  need_recording_viewport = false;
}

// static
bool PaintTimingVisualizer::IsTracingEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("loading", &enabled);
  return enabled;
}

}  // namespace blink
