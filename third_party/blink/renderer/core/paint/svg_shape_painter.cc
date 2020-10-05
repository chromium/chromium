// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_shape_painter.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_marker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_marker_data.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/svg_container_painter.h"
#include "third_party/blink/renderer/core/paint/svg_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

namespace blink {

static bool SetupNonScalingStrokeContext(
    AffineTransform& stroke_transform,
    GraphicsContextStateSaver& state_saver) {
  if (!stroke_transform.IsInvertible())
    return false;

  state_saver.Save();
  state_saver.Context().ConcatCTM(stroke_transform.Inverse());
  return true;
}

static SkPathFillType FillRuleFromStyle(const PaintInfo& paint_info,
                                        const SVGComputedStyle& svg_style) {
  return WebCoreWindRuleToSkFillType(paint_info.IsRenderingClipPathAsMaskImage()
                                         ? svg_style.ClipRule()
                                         : svg_style.FillRule());
}

void SVGShapePainter::Paint(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground ||
      layout_svg_shape_.StyleRef().Visibility() != EVisibility::kVisible ||
      layout_svg_shape_.IsShapeEmpty())
    return;

  if (SVGModelObjectPainter::CanUseCullRect(layout_svg_shape_.StyleRef())) {
    if (!paint_info.GetCullRect().IntersectsTransformed(
            layout_svg_shape_.LocalSVGTransform(),
            layout_svg_shape_.VisualRectInLocalSVGCoordinates()))
      return;
  }
  // Shapes cannot have children so do not call TransformCullRect.

  ScopedSVGTransformState transform_state(paint_info, layout_svg_shape_);
  {
    ScopedSVGPaintState paint_state(layout_svg_shape_, paint_info);
    if (!DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, layout_svg_shape_, paint_info.phase)) {
      SVGModelObjectPainter::RecordHitTestData(layout_svg_shape_, paint_info);
      SVGDrawingRecorder recorder(paint_info.context, layout_svg_shape_,
                                  paint_info.phase);
      const SVGComputedStyle& svg_style =
          layout_svg_shape_.StyleRef().SvgStyle();

      bool should_anti_alias = svg_style.ShapeRendering() != SR_CRISPEDGES &&
                               svg_style.ShapeRendering() != SR_OPTIMIZESPEED;

      for (int i = 0; i < 3; i++) {
        switch (svg_style.PaintOrderType(i)) {
          case PT_FILL: {
            PaintFlags fill_flags;
            if (!SVGObjectPainter(layout_svg_shape_)
                     .PreparePaint(paint_info, layout_svg_shape_.StyleRef(),
                                   kApplyToFillMode, fill_flags))
              break;
            fill_flags.setAntiAlias(should_anti_alias);
            FillShape(paint_info.context, fill_flags,
                      FillRuleFromStyle(paint_info, svg_style));
            break;
          }
          case PT_STROKE:
            if (svg_style.HasVisibleStroke()) {
              GraphicsContextStateSaver state_saver(paint_info.context, false);
              AffineTransform non_scaling_transform;
              const AffineTransform* additional_paint_server_transform =
                  nullptr;

              if (layout_svg_shape_.HasNonScalingStroke()) {
                non_scaling_transform =
                    layout_svg_shape_.NonScalingStrokeTransform();
                if (!SetupNonScalingStrokeContext(non_scaling_transform,
                                                  state_saver))
                  return;

                // Non-scaling stroke needs to reset the transform back to the
                // host transform.
                additional_paint_server_transform = &non_scaling_transform;
              }

              PaintFlags stroke_flags;
              if (!SVGObjectPainter(layout_svg_shape_)
                       .PreparePaint(paint_info, layout_svg_shape_.StyleRef(),
                                     kApplyToStrokeMode, stroke_flags,
                                     additional_paint_server_transform))
                break;
              stroke_flags.setAntiAlias(should_anti_alias);

              StrokeData stroke_data;
              SVGLayoutSupport::ApplyStrokeStyleToStrokeData(
                  stroke_data, layout_svg_shape_.StyleRef(), layout_svg_shape_,
                  layout_svg_shape_.DashScaleFactor());
              stroke_data.SetupPaint(&stroke_flags);

              StrokeShape(paint_info.context, stroke_flags);
            }
            break;
          case PT_MARKERS:
            PaintMarkers(paint_info);
            break;
          default:
            NOTREACHED();
            break;
        }
      }
    }
  }

  SVGModelObjectPainter(layout_svg_shape_).PaintOutline(paint_info);
}

class PathWithTemporaryWindingRule {
 public:
  PathWithTemporaryWindingRule(Path& path, SkPathFillType fill_type)
      : path_(const_cast<SkPath&>(path.GetSkPath())) {
    saved_fill_type_ = path_.getFillType();
    path_.setFillType(fill_type);
  }
  ~PathWithTemporaryWindingRule() { path_.setFillType(saved_fill_type_); }

  const SkPath& GetSkPath() const { return path_; }

 private:
  SkPath& path_;
  SkPathFillType saved_fill_type_;
};

void SVGShapePainter::FillShape(GraphicsContext& context,
                                const PaintFlags& flags,
                                SkPathFillType fill_type) {
  switch (layout_svg_shape_.GeometryCodePath()) {
    case kRectGeometryFastPath:
      context.DrawRect(layout_svg_shape_.ObjectBoundingBox(), flags,
                       DarkModeFilter::ElementRole::kSVG);
      break;
    case kEllipseGeometryFastPath:
      context.DrawOval(layout_svg_shape_.ObjectBoundingBox(), flags,
                       DarkModeFilter::ElementRole::kSVG);
      break;
    default: {
      PathWithTemporaryWindingRule path_with_winding(
          layout_svg_shape_.GetPath(), fill_type);
      context.DrawPath(path_with_winding.GetSkPath(), flags,
                       DarkModeFilter::ElementRole::kSVG);
    }
  }
  PaintTiming& timing = PaintTiming::From(
      layout_svg_shape_.GetElement()->GetDocument().TopDocument());
  timing.MarkFirstContentfulPaint();
}

void SVGShapePainter::StrokeShape(GraphicsContext& context,
                                  const PaintFlags& flags) {
  DCHECK(layout_svg_shape_.StyleRef().SvgStyle().HasVisibleStroke());

  switch (layout_svg_shape_.GeometryCodePath()) {
    case kRectGeometryFastPath:
      context.DrawRect(layout_svg_shape_.ObjectBoundingBox(), flags,
                       DarkModeFilter::ElementRole::kSVG);
      break;
    case kEllipseGeometryFastPath:
      context.DrawOval(layout_svg_shape_.ObjectBoundingBox(), flags,
                       DarkModeFilter::ElementRole::kSVG);
      break;
    default:
      DCHECK(layout_svg_shape_.HasPath());
      const Path* use_path = &layout_svg_shape_.GetPath();
      if (layout_svg_shape_.HasNonScalingStroke())
        use_path = &layout_svg_shape_.NonScalingStrokePath();
      context.DrawPath(use_path->GetSkPath(), flags,
                       DarkModeFilter::ElementRole::kSVG);
  }
  PaintTiming& timing = PaintTiming::From(
      layout_svg_shape_.GetElement()->GetDocument().TopDocument());
  timing.MarkFirstContentfulPaint();
}

void SVGShapePainter::PaintMarkers(const PaintInfo& paint_info) {
  const Vector<MarkerPosition>* marker_positions =
      layout_svg_shape_.MarkerPositions();
  if (!marker_positions || marker_positions->IsEmpty())
    return;

  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(layout_svg_shape_);
  if (!resources)
    return;

  LayoutSVGResourceMarker* marker_start = resources->MarkerStart();
  LayoutSVGResourceMarker* marker_mid = resources->MarkerMid();
  LayoutSVGResourceMarker* marker_end = resources->MarkerEnd();
  if (!marker_start && !marker_mid && !marker_end)
    return;

  const float stroke_width = layout_svg_shape_.StrokeWidthForMarkerUnits();

  for (const MarkerPosition& marker_position : *marker_positions) {
    if (LayoutSVGResourceMarker* marker = marker_position.SelectMarker(
            marker_start, marker_mid, marker_end)) {
      PaintMarker(paint_info, *marker, marker_position, stroke_width);
    }
  }
}

void SVGShapePainter::PaintMarker(const PaintInfo& paint_info,
                                  LayoutSVGResourceMarker& marker,
                                  const MarkerPosition& position,
                                  float stroke_width) {
  marker.ClearInvalidationMask();

  if (!marker.ShouldPaint())
    return;

  AffineTransform transform =
      marker.MarkerTransformation(position, stroke_width);

  cc::PaintCanvas* canvas = paint_info.context.Canvas();

  canvas->save();
  canvas->concat(AffineTransformToSkMatrix(transform));
  if (SVGLayoutSupport::IsOverflowHidden(marker))
    canvas->clipRect(marker.Viewport());

  PaintRecordBuilder builder(nullptr, &paint_info.context);
  PaintInfo marker_paint_info(builder.Context(), paint_info);
  // It's expensive to track the transformed paint cull rect for each
  // marker so just disable culling. The shape paint call will already
  // be culled if it is outside the paint info cull rect.
  marker_paint_info.ApplyInfiniteCullRect();

  SVGContainerPainter(marker).Paint(marker_paint_info);
  builder.EndRecording(*canvas);

  canvas->restore();
}

}  // namespace blink
