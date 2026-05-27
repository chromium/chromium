// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/embedded_content_painter.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "cc/layers/view_transition_content_layer.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/embedded_content_view.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/core/paint/replaced_painter.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

namespace {
scoped_refptr<cc::ViewTransitionContentLayer> GetSubframeSnapshotLayer(
    const EmbeddedContentView& embedded_content_view,
    PaintPhase phase) {
  if (phase != PaintPhase::kForeground) {
    return nullptr;
  }

  auto* local_frame_view = DynamicTo<LocalFrameView>(embedded_content_view);
  if (!local_frame_view) {
    return nullptr;
  }

  auto* transition = ViewTransitionUtils::GetTransition(
      *local_frame_view->GetFrame().GetDocument());
  if (!transition) {
    return nullptr;
  }

  return transition->GetScopeSnapshotLayer();
}

}  // namespace

void EmbeddedContentPainter::PaintReplaced(const PaintInfo& paint_info,
                                           const PhysicalOffset& paint_offset) {
  EmbeddedContentView* embedded_content_view =
      layout_embedded_content_.GetEmbeddedContentView();
  if (!embedded_content_view)
    return;

  std::optional<ScopedPaintChunkProperties> removed_svg_filter_paint =
      RemoveSvgFilterPaint(layout_embedded_content_, paint_info);

  // Apply the translation to offset the content within the object's border-box
  // only if we're not using a transform node for this. If the frame size is
  // frozen then |ReplacedContentTransform| is used instead.
  gfx::Point paint_location;
  if (!layout_embedded_content_.FrozenFrameSize().has_value()) {
    paint_location = ToRoundedPoint(
        paint_offset + layout_embedded_content_.ReplacedContentRect().offset);
  }

  gfx::Vector2d view_paint_offset =
      paint_location - embedded_content_view->FrameRect().origin();
  CullRect adjusted_cull_rect = paint_info.GetCullRect();
  adjusted_cull_rect.Move(-view_paint_offset);
  embedded_content_view->Paint(paint_info, adjusted_cull_rect,
                               view_paint_offset);

  // During a ViewTransition in a LocalFrame sub-frame, we need to keep painting
  // the old Document's last frame until the new Document is ready to start
  // rendering.
  //
  // Note: The iframe is throttled for the duration the new state is not ready
  // to display. This is true for both same-document transitions (the update
  // callback is running) and cross-document transitions (the new Document is
  // render-blocked).
  //
  // When the iframe is throttled, the embedded content view will not paint
  // anything but we still paint this foreign layer to keep displaying the old
  // content.
  if (auto layer =
          GetSubframeSnapshotLayer(*embedded_content_view, paint_info.phase)) {
    GraphicsContext& context = paint_info.context;
    layer->SetBounds(embedded_content_view->FrameRect().size());
    layer->SetIsDrawable(true);
    RecordForeignLayer(context, layout_embedded_content_,
                       DisplayItem::kForeignLayerViewTransitionContent,
                       std::move(layer), paint_location);
  }
}

// static
std::optional<ScopedPaintChunkProperties>
EmbeddedContentPainter::RemoveSvgFilterPaint(
    const LayoutEmbeddedContent& layout_embedded_content,
    const PaintInfo& paint_info) {
  // First, we gate the removal of the reference filter paint behind a feature
  // flag. This is differentiated per-type of embedded content.
  const EmbeddedContentView* embedded_content_view =
      layout_embedded_content.GetEmbeddedContentView();
  if (!embedded_content_view ||
      !base::FeatureList::IsEnabled(features::kPreventSvgFilterPaint)) {
    return std::nullopt;
  }
  DisplayItem::Type display_item_type = DisplayItem::kUninitializedType;
  switch (embedded_content_view->SvgFilterPaintedCounter()) {
    case mojom::blink::WebFeature::kSvgFilterPaintedOnLocalFrame:
      // We only care about restricted local frames.
      if (!features::kPreventSvgFilterPaintOnLocalFrameRestricted.Get() ||
          !To<LocalFrameView>(embedded_content_view)
               ->GetFrame()
               .IsCrossOriginToParentOrOuterDocument()) {
        return std::nullopt;
      }
      // We cannot remove the filter here as that must be done during pre-paint
      // in PaintPropertyTreeBuilder::SetupContextForFrame, but we still want to
      // call CountDeprecation below, so we keep kUninitializedType as the type.
      break;
    case mojom::blink::WebFeature::kSvgFilterPaintedOnRemoteFrame:
      if (!features::kPreventSvgFilterPaintOnRemoteFrame.Get()) {
        return std::nullopt;
      }
      display_item_type = DisplayItem::kForeignLayerRemoteFrame;
      break;
    case mojom::blink::WebFeature::kSvgFilterPaintedOnWebPlugin:
      if (!features::kPreventSvgFilterPaintOnWebPlugin.Get()) {
        return std::nullopt;
      }
      display_item_type = DisplayItem::kWebPlugin;
      break;
    default:
      NOTREACHED();
  }

  // Then find the parent effect to overwrite with (if it exists).
  const blink::EffectPaintPropertyNode* candidate_effect =
      PaintPropertyTreeBuilder::GetFirstParentEffectWithoutReferenceFilter(
          &paint_info.context.GetPaintController()
               .CurrentPaintChunkProperties()
               .Effect());

  // We can exit early if there is no effect with a reference filter.
  if (!candidate_effect) {
    return std::nullopt;
  }
  layout_embedded_content.GetDocument().CountDeprecation(
      mojom::blink::WebFeature::kPreventSvgFilterPaint);

  // We can exit at this point if we don't have a targetable type.
  if (display_item_type == DisplayItem::kUninitializedType) {
    return std::nullopt;
  }

  // Finally we emplace the (revised) scoped properties.
  return std::optional<ScopedPaintChunkProperties>{
      std::in_place, paint_info.context.GetPaintController(), *candidate_effect,
      layout_embedded_content, display_item_type};
}

}  // namespace blink
