// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_shape_painter.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_marker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_marker_data.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/svg_container_painter.h"
#include "third_party/blink/renderer/core/paint/svg_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

static absl::optional<AffineTransform> SetupNonScalingStrokeContext(
    const LayoutSVGShape& layout_svg_shape,
    GraphicsContextStateSaver& state_saver) {
  const AffineTransform& non_scaling_stroke_transform =
      layout_svg_shape.NonScalingStrokeTransform();
  if (!non_scaling_stroke_transform.IsInvertible())
    return absl::nullopt;
  state_saver.Save();
  state_saver.Context().ConcatCTM(non_scaling_stroke_transform.Inverse());
  return non_scaling_stroke_transform;
}

static SkPathFillType FillRuleFromStyle(const PaintInfo& paint_info,
                                        const ComputedStyle& style) {
  return WebCoreWindRuleToSkFillType(paint_info.IsRenderingClipPathAsMaskImage()
                                         ? style.ClipRule()
                                         : style.FillRule());
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
    SVGModelObjectPainter::RecordHitTestData(layout_svg_shape_, paint_info);
    SVGModelObjectPainter::RecordRegionCaptureData(layout_svg_shape_,
                                                   paint_info);
    if (!DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, layout_svg_shape_, paint_info.phase)) {
      SVGDrawingRecorder recorder(paint_info.context, layout_svg_shape_,
                                  paint_info.phase);
      const ComputedStyle& style = layout_svg_shape_.StyleRef();

      bool should_anti_alias =
          style.ShapeRendering() != EShapeRendering::kCrispedges &&
          style.ShapeRendering() != EShapeRendering::kOptimizespeed;

      for (int i = 0; i < 3; i++) {
        switch (style.PaintOrderType(i)) {
          case PT_FILL: {
            cc::PaintFlags fill_flags;
            if (!SVGObjectPainter(layout_svg_shape_)
                     .PreparePaint(paint_info.context,
                                   paint_info.IsRenderingClipPathAsMaskImage(),
                                   style, kApplyToFillMode, fill_flags)) {
              break;
            }
            fill_flags.setAntiAlias(should_anti_alias);
            FillShape(paint_info.context, fill_flags,
                      FillRuleFromStyle(paint_info, style));
            break;
          }
          case PT_STROKE:
            if (style.HasVisibleStroke()) {
              GraphicsContextStateSaver state_saver(paint_info.context, false);
              absl::optional<AffineTransform> non_scaling_transform;

              if (layout_svg_shape_.HasNonScalingStroke()) {
                // Non-scaling stroke needs to reset the transform back to the
                // host transform.
                non_scaling_transform = SetupNonScalingStrokeContext(
                    layout_svg_shape_, state_saver);
                if (!non_scaling_transform)
                  return;
              }

              cc::PaintFlags stroke_flags;
              if (!SVGObjectPainter(layout_svg_shape_)
                       .PreparePaint(
                           paint_info.context,
                           paint_info.IsRenderingClipPathAsMaskImage(), style,
                           kApplyToStrokeMode, stroke_flags,
                           base::OptionalToPtr(non_scaling_transform))) {
                break;
              }
              stroke_flags.setAntiAlias(should_anti_alias);

              StrokeData stroke_data;
              SVGLayoutSupport::ApplyStrokeStyleToStrokeData(
                  stroke_data, style, layout_svg_shape_,
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
  STACK_ALLOCATED();

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
                                const cc::PaintFlags& flags,
                                SkPathFillType fill_type) {
  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      layout_svg_shape_.StyleRef(), DarkModeFilter::ElementRole::kSVG));
  switch (layout_svg_shape_.GeometryCodePath()) {
    case kRectGeometryFastPath:
      context.DrawRect(
          gfx::RectFToSkRect(layout_svg_shape_.ObjectBoundingBox()), flags,
          auto_dark_mode);
      break;
    case kEllipseGeometryFastPath:
      context.DrawOval(
          gfx::RectFToSkRect(layout_svg_shape_.ObjectBoundingBox()), flags,
          auto_dark_mode);
      break;
    default: {
      PathWithTemporaryWindingRule path_with_winding(
          layout_svg_shape_.GetPath(), fill_type);
      context.DrawPath(path_with_winding.GetSkPath(), flags, auto_dark_mode);
    }
  }
  PaintTiming& timing = PaintTiming::From(layout_svg_shape_.GetDocument());
  timing.MarkFirstContentfulPaint();
}

void SVGShapePainter::StrokeShape(GraphicsContext& context,
                                  const cc::PaintFlags& flags) {
  DCHECK(layout_svg_shape_.StyleRef().HasVisibleStroke());

  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      layout_svg_shape_.StyleRef(), DarkModeFilter::ElementRole::kSVG));

  switch (layout_svg_shape_.GeometryCodePath()) {
    case kRectGeometryFastPath:
      context.DrawRect(
          gfx::RectFToSkRect(layout_svg_shape_.ObjectBoundingBox()), flags,
          auto_dark_mode);
      break;
    case kEllipseGeometryFastPath:
      context.DrawOval(
          gfx::RectFToSkRect(layout_svg_shape_.ObjectBoundingBox()), flags,
          auto_dark_mode);
      break;
    default:
      DCHECK(layout_svg_shape_.HasPath());
      const Path* use_path = &layout_svg_shape_.GetPath();
      if (layout_svg_shape_.HasNonScalingStroke())
        use_path = &layout_svg_shape_.NonScalingStrokePath();
      context.DrawPath(use_path->GetSkPath(), flags, auto_dark_mode);
  }
  PaintTiming& timing = PaintTiming::From(layout_svg_shape_.GetDocument());
  timing.MarkFirstContentfulPaint();
}

void SVGShapePainter::PaintMarkers(const PaintInfo& paint_info) {
  const Vector<MarkerPosition>* marker_positions =
      layout_svg_shape_.MarkerPositions();
  if (!marker_positions || marker_positions->empty())
    return;
  SVGResourceClient* client = SVGResources::GetClient(layout_svg_shape_);
  const ComputedStyle& style = layout_svg_shape_.StyleRef();
  auto* marker_start = GetSVGResourceAsType<LayoutSVGResourceMarker>(
      *client, style.MarkerStartResource());
  auto* marker_mid = GetSVGResourceAsType<LayoutSVGResourceMarker>(
      *client, style.MarkerMidResource());
  auto* marker_end = GetSVGResourceAsType<LayoutSVGResourceMarker>(
      *client, style.MarkerEndResource());
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
    canvas->clipRect(gfx::RectFToSkRect(marker.Viewport()));
  auto* builder = MakeGarbageCollected<PaintRecordBuilder>(paint_info.context);
  PaintInfo marker_paint_info(builder->Context(), paint_info);
  // It's expensive to track the transformed paint cull rect for each
  // marker so just disable culling. The shape paint call will already
  // be culled if it is outside the paint info cull rect.
  marker_paint_info.ApplyInfiniteCullRect();

  SVGContainerPainter(marker).Paint(marker_paint_info);
  builder->EndRecording(*canvas);

  canvas->restore();
}

}  // namespace blink
