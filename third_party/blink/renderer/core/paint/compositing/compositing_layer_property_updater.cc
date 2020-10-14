// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_layer_property_updater.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/paint/fragment_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace {
enum class ScrollbarOrCorner {
  kHorizontalScrollbar,
  kVerticalScrollbar,
  kScrollbarCorner,
};
}

namespace blink {

void CompositingLayerPropertyUpdater::Update(const LayoutObject& object) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  if (!object.HasLayer())
    return;
  const auto* paint_layer = ToLayoutBoxModelObject(object).Layer();
  const auto* mapping = paint_layer->GetCompositedLayerMapping();
  if (!mapping)
    return;

  const FragmentData& fragment_data = object.FirstFragment();
  DCHECK(fragment_data.HasLocalBorderBoxProperties());
  // SPv1 compositing forces single fragment for directly composited elements.
  DCHECK(!fragment_data.NextFragment() ||
         // We create multiple fragments for composited repeating fixed-position
         // during printing.
         object.GetDocument().Printing() ||
         !object.CanBeCompositedForDirectReasons());

  // TODO(chrishtr): this should never happen, but does in practice and
  // we haven't been able to find all of the cases where it happens yet.
  // Once we find more of them, try to remove this.
  if (!fragment_data.HasLocalBorderBoxProperties())
    return;

  PhysicalOffset layout_snapped_paint_offset =
      fragment_data.PaintOffset() - paint_layer->SubpixelAccumulation();
  IntPoint snapped_paint_offset = RoundedIntPoint(layout_snapped_paint_offset);

#if DCHECK_IS_ON()
  // A layer without visible contents can be composited due to animation.
  // Since the layer itself has no visible subtree, there is no guarantee
  // that all of its ancestors have a visible subtree. An ancestor with no
  // visible subtree can be non-composited despite we expected it to, this
  // resulted in the paint offset used by CompositedLayerMapping to mismatch.
  bool subpixel_accumulation_may_be_bogus = paint_layer->SubtreeIsInvisible();
  if (!subpixel_accumulation_may_be_bogus &&
      layout_snapped_paint_offset != PhysicalOffset(snapped_paint_offset)) {
    // TODO(crbug.com/925377): Fix the root cause.
    DLOG(ERROR) << "Paint offset pixel snapping error for " << object
                << " expected: " << snapped_paint_offset
                << " actual: " << layout_snapped_paint_offset;
  }
#endif

  base::Optional<PropertyTreeStateOrAlias> container_layer_state;
  auto SetContainerLayerState =
      [&fragment_data, &snapped_paint_offset,
       &container_layer_state](GraphicsLayer* graphics_layer) {
        if (graphics_layer) {
          if (!container_layer_state)
            container_layer_state = fragment_data.LocalBorderBoxProperties();
          graphics_layer->SetLayerState(
              *container_layer_state,
              snapped_paint_offset + graphics_layer->OffsetFromLayoutObject());
        }
      };
  SetContainerLayerState(mapping->MainGraphicsLayer());
  SetContainerLayerState(mapping->DecorationOutlineLayer());

  bool is_root_scroller =
      CompositingReasonFinder::RequiresCompositingForRootScroller(*paint_layer);

  auto SetContainerLayerStateForScrollbars =
      [&object, &is_root_scroller, &fragment_data, &snapped_paint_offset,
       &container_layer_state](GraphicsLayer* graphics_layer,
                               ScrollbarOrCorner scrollbar_or_corner) {
        if (!graphics_layer)
          return;
        PropertyTreeStateOrAlias scrollbar_layer_state =
            container_layer_state.value_or(
                fragment_data.LocalBorderBoxProperties());
        // OverflowControlsClip should be applied within the scrollbar layers.
        if (const auto* properties = fragment_data.PaintProperties()) {
          if (const auto* clip = properties->OverflowControlsClip())
            scrollbar_layer_state.SetClip(*clip);

          if (scrollbar_or_corner == ScrollbarOrCorner::kHorizontalScrollbar) {
            if (const auto* effect = properties->HorizontalScrollbarEffect()) {
              scrollbar_layer_state.SetEffect(*effect);
            }
          } else if (scrollbar_or_corner ==
                     ScrollbarOrCorner::kVerticalScrollbar) {
            if (const auto* effect = properties->VerticalScrollbarEffect())
              scrollbar_layer_state.SetEffect(*effect);
          }
        }

        if (is_root_scroller) {
          // The root scrollbar needs to use a transform node above the
          // overscroll elasticity layer because the root scrollbar should not
          // bounce with overscroll.
          const auto* frame_view = object.GetFrameView();
          DCHECK(frame_view);
          const auto* page = frame_view->GetPage();
          const auto& viewport = page->GetVisualViewport();
          if (const auto* transform =
                  viewport.GetOverscrollElasticityTransformNode()) {
            DCHECK(transform->Parent());
            scrollbar_layer_state.SetTransform(*transform->Parent());
          }
        }

        graphics_layer->SetLayerState(
            scrollbar_layer_state,
            snapped_paint_offset + graphics_layer->OffsetFromLayoutObject());
      };

  SetContainerLayerStateForScrollbars(mapping->LayerForHorizontalScrollbar(),
                                      ScrollbarOrCorner::kHorizontalScrollbar);
  SetContainerLayerStateForScrollbars(mapping->LayerForVerticalScrollbar(),
                                      ScrollbarOrCorner::kVerticalScrollbar);
  SetContainerLayerStateForScrollbars(mapping->LayerForScrollCorner(),
                                      ScrollbarOrCorner::kScrollbarCorner);

  if (mapping->ScrollingContentsLayer()) {
    // See comments for ScrollTranslation in object_paint_properties.h for the
    // reason of adding ScrollOrigin().
    auto contents_paint_offset =
        snapped_paint_offset + ToLayoutBox(object).ScrollOrigin();
    auto SetScrollingContentsLayerState = [&fragment_data,
                                           &contents_paint_offset](
                                              GraphicsLayer* graphics_layer) {
      if (graphics_layer) {
        graphics_layer->SetLayerState(
            fragment_data.ContentsProperties(),
            contents_paint_offset + graphics_layer->OffsetFromLayoutObject());
      }
    };
    SetScrollingContentsLayerState(mapping->ScrollingContentsLayer());
    SetScrollingContentsLayerState(mapping->ForegroundLayer());
  } else {
    SetContainerLayerState(mapping->ForegroundLayer());
  }

  auto* main_graphics_layer = mapping->MainGraphicsLayer();
  if (main_graphics_layer->ContentsLayer()) {
    IntPoint offset;
    // The offset should be zero when the layer has ReplacedContentTransform,
    // because the offset has been baked into ReplacedContentTransform.
    if (!fragment_data.PaintProperties() ||
        !fragment_data.PaintProperties()->ReplacedContentTransform()) {
      offset = main_graphics_layer->ContentsRect().Location() +
               main_graphics_layer->GetOffsetFromTransformNode();
    }
    main_graphics_layer->SetContentsLayerState(
        fragment_data.ContentsProperties(), offset);
  }

  if (auto* squashing_layer = mapping->NonScrollingSquashingLayer()) {
    auto state = fragment_data.PreEffectProperties();
    // The squashing layer's ClippingContainer is the common ancestor of clip
    // state of all squashed layers, so we should use its clip state. This skips
    // any control clips on the squashing layer's object which should not apply
    // on squashed layers.
    const auto* clipping_container = paint_layer->ClippingContainer();
    state.SetClip(
        clipping_container
            ? clipping_container->FirstFragment().ContentsProperties().Clip()
            : ClipPaintPropertyNode::Root());
    squashing_layer->SetLayerState(
        state, snapped_paint_offset +
                   mapping->NonScrollingSquashingLayerOffsetFromLayoutObject());
  }

  if (auto* mask_layer = mapping->MaskLayer()) {
    auto state = fragment_data.LocalBorderBoxProperties();
    const auto* properties = fragment_data.PaintProperties();
    DCHECK(properties);
    DCHECK(properties->Mask() || properties->ClipPathMask());
    DCHECK(properties->MaskClip());
    state.SetEffect(properties->Mask() ? *properties->Mask()
                                       : *properties->ClipPathMask());
    state.SetClip(*properties->MaskClip());

    mask_layer->SetLayerState(
        state, snapped_paint_offset + mask_layer->OffsetFromLayoutObject());
  }

  if (RuntimeEnabledFeatures::CompositeSVGEnabled()) {
    if (object.IsSVGRoot()) {
      main_graphics_layer->SetShouldCreateLayersAfterPaint(
          ToLayoutSVGRoot(object).HasDescendantCompositingReasons() &&
          main_graphics_layer->PaintsContentOrHitTest());
    }
  }
}

}  // namespace blink
