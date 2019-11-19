// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/details_marker_painter.h"

#include "third_party/blink/renderer/core/layout/layout_details_marker.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/path.h"

namespace blink {

void DetailsMarkerPainter::Paint(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground ||
      layout_details_marker_.StyleRef().Visibility() != EVisibility::kVisible) {
    BlockPainter(layout_details_marker_).Paint(paint_info);
    return;
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_details_marker_, paint_info.phase))
    return;

  ScopedPaintState paint_state(layout_details_marker_, paint_info);
  if (!paint_state.LocalRectIntersectsCullRect(
          layout_details_marker_.PhysicalVisualOverflowRect()))
    return;

  const auto& local_paint_info = paint_state.GetPaintInfo();
  DrawingRecorder recorder(local_paint_info.context, layout_details_marker_,
                           local_paint_info.phase);
  const Color color(layout_details_marker_.ResolveColor(GetCSSPropertyColor()));
  local_paint_info.context.SetFillColor(color);

  auto box_origin = paint_state.PaintOffset();
  box_origin += PhysicalOffset(
      layout_details_marker_.BorderLeft() +
          layout_details_marker_.PaddingLeft(),
      layout_details_marker_.BorderTop() + layout_details_marker_.PaddingTop());
  local_paint_info.context.FillPath(GetPath(box_origin));
}

static Path CreatePath(const FloatPoint* path) {
  Path result;
  result.MoveTo(FloatPoint(path[0].X(), path[0].Y()));
  for (int i = 1; i < 4; ++i)
    result.AddLineTo(FloatPoint(path[i].X(), path[i].Y()));
  return result;
}

static Path CreateDownArrowPath() {
  FloatPoint points[4] = {FloatPoint(0.0f, 0.07f), FloatPoint(0.5f, 0.93f),
                          FloatPoint(1.0f, 0.07f), FloatPoint(0.0f, 0.07f)};
  return CreatePath(points);
}

static Path CreateUpArrowPath() {
  FloatPoint points[4] = {FloatPoint(0.0f, 0.93f), FloatPoint(0.5f, 0.07f),
                          FloatPoint(1.0f, 0.93f), FloatPoint(0.0f, 0.93f)};
  return CreatePath(points);
}

static Path CreateLeftArrowPath() {
  FloatPoint points[4] = {FloatPoint(1.0f, 0.0f), FloatPoint(0.14f, 0.5f),
                          FloatPoint(1.0f, 1.0f), FloatPoint(1.0f, 0.0f)};
  return CreatePath(points);
}

static Path CreateRightArrowPath() {
  FloatPoint points[4] = {FloatPoint(0.0f, 0.0f), FloatPoint(0.86f, 0.5f),
                          FloatPoint(0.0f, 1.0f), FloatPoint(0.0f, 0.0f)};
  return CreatePath(points);
}

Path DetailsMarkerPainter::GetCanonicalPath() const {
  switch (layout_details_marker_.GetOrientation()) {
    case LayoutDetailsMarker::kLeft:
      return CreateLeftArrowPath();
    case LayoutDetailsMarker::kRight:
      return CreateRightArrowPath();
    case LayoutDetailsMarker::kUp:
      return CreateUpArrowPath();
    case LayoutDetailsMarker::kDown:
      return CreateDownArrowPath();
  }

  return Path();
}

Path DetailsMarkerPainter::GetPath(const PhysicalOffset& origin) const {
  Path result = GetCanonicalPath();
  result.Transform(AffineTransform().Scale(
      layout_details_marker_.ContentWidth().ToFloat(),
      layout_details_marker_.ContentHeight().ToFloat()));
  result.Translate(FloatSize(origin));
  return result;
}

}  // namespace blink
