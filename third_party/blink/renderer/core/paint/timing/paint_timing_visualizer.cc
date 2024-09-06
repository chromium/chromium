// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing_visualizer.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

void CreateQuad(TracedValue* value, const char* name, const gfx::QuadF& quad) {
  value->BeginArray(name);
  value->PushDouble(quad.p1().x());
  value->PushDouble(quad.p1().y());
  value->PushDouble(quad.p2().x());
  value->PushDouble(quad.p2().y());
  value->PushDouble(quad.p3().x());
  value->PushDouble(quad.p3().y());
  value->PushDouble(quad.p4().x());
  value->PushDouble(quad.p4().y());
  value->EndArray();
}

}  // namespace

PaintTimingVisualizer::PaintTimingVisualizer() {
  trace_event::AddEnabledStateObserver(this);
}

PaintTimingVisualizer::~PaintTimingVisualizer() {
  trace_event::RemoveEnabledStateObserver(this);
}

void PaintTimingVisualizer::RecordRects(const gfx::Rect& rect,
                                        std::unique_ptr<TracedValue>& value) {
  CreateQuad(value.get(), "rect", gfx::QuadF(gfx::RectF(rect)));
}
void PaintTimingVisualizer::RecordObject(const LayoutObject& object,
                                         std::unique_ptr<TracedValue>& value) {
  value->SetString("object_name", object.GetName());
  DCHECK(object.GetFrame());
  value->SetString("frame", GetFrameIdForTracing(object.GetFrame()));
  value->SetBoolean("is_in_main_frame", object.GetFrame()->IsMainFrame());
  value->SetBoolean("is_in_outermost_main_frame",
                    object.GetFrame()->IsOutermostMainFrame());
  if (object.GetNode())
    value->SetInteger("dom_node_id", object.GetNode()->GetDomNodeId());
}

void PaintTimingVisualizer::DumpTextDebuggingRect(const LayoutObject& object,
                                                  const gfx::RectF& rect) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  RecordObject(object, value);
  RecordRects(gfx::ToRoundedRect(rect), value);
  value->SetBoolean("is_aggregation_text", true);
  value->SetBoolean("is_svg", object.IsSVG());
  DumpTrace(std::move(value));
}

void PaintTimingVisualizer::DumpImageDebuggingRect(const LayoutObject& object,
                                                   const gfx::RectF& rect,
                                                   bool is_loaded,
                                                   const KURL& url) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  RecordObject(object, value);
  RecordRects(gfx::ToRoundedRect(rect), value);
  value->SetBoolean("is_image", true);
  value->SetBoolean("is_svg", object.IsSVG());
  value->SetBoolean("is_image_loaded", is_loaded);
  value->SetString("image_url", url.StrippedForUseAsReferrer());
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
  if (!frame_view.GetFrame().IsOutermostMainFrame())
    return;
  ScrollableArea* scrollable_area = frame_view.GetScrollableArea();
  DCHECK(scrollable_area);
  gfx::Rect viewport_rect = scrollable_area->VisibleContentRect();

  FloatClipRect float_clip_visual_rect((gfx::RectF(viewport_rect)));
  gfx::RectF float_visual_rect =
      frame_view.GetPaintTimingDetector().BlinkSpaceToDIPs(
          float_clip_visual_rect.Rect());

  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  CreateQuad(value.get(), "viewport_rect", gfx::QuadF(float_visual_rect));
  value->SetDouble("dpr", frame_view.GetFrame().DevicePixelRatio());
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

void PaintTimingVisualizer::OnTraceLogEnabled() {
  need_recording_viewport = true;
}

void PaintTimingVisualizer::OnTraceLogDisabled() {}

}  // namespace blink
