// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/view_painter.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/box_background_paint_context.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// Behind the root element of the main frame of the page, there is an infinite
// canvas. This is by default white, but it can be overridden by
// BaseBackgroundColor on the LocalFrameView.
// https://drafts.fxtf.org/compositing/#rootgroup
void ViewPainter::PaintRootGroup(const PaintInfo& paint_info,
                                 const gfx::Rect& pixel_snapped_background_rect,
                                 const Document& document,
                                 const DisplayItemClient& client,
                                 const PropertyTreeStateOrAlias& state) {
  const LayoutView& layout_view = GetLayoutView();
  if (!layout_view.GetFrameView()->ShouldPaintBaseBackgroundColor()) {
    return;
  }

  Color base_background_color =
      layout_view.GetFrameView()->BaseBackgroundColor();
  if (document.Printing() && base_background_color == Color::kWhite) {
    // Leave a transparent background, assuming the paper or the PDF viewer
    // background is white by default. This allows further customization of the
    // background, e.g. in the case of https://crbug.com/498892.
    return;
  }

  bool should_clear_canvas =
      document.GetSettings() &&
      document.GetSettings()->GetShouldClearDocumentBackground();

  ScopedPaintChunkProperties frame_view_background_state(
      paint_info.context.GetPaintController(), state, client,
      DisplayItem::kDocumentRootBackdrop);
  GraphicsContext& context = paint_info.context;
  if (!DrawingRecorder::UseCachedDrawingIfPossible(
          context, client, DisplayItem::kDocumentRootBackdrop)) {
    DrawingRecorder recorder(context, client,
                             DisplayItem::kDocumentRootBackdrop,
                             pixel_snapped_background_rect);
    context.FillRect(
        pixel_snapped_background_rect, base_background_color,
        PaintAutoDarkMode(box_fragment_.Style(),
                          DarkModeFilter::ElementRole::kBackground),
        should_clear_canvas ? SkBlendMode::kSrc : SkBlendMode::kSrcOver);
  }
}

void ViewPainter::PaintBoxDecorationBackground(const PaintInfo& paint_info) {
  if (box_fragment_.Style().UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  if (box_fragment_.IsPaginatedRoot()) {
    // When paginated, the background is painted for each individual page. The
    // @page background is painted for the kPageContainer. The root view
    // fragment itself paints no background.
    return;
  }

  const LayoutView& layout_view = GetLayoutView();
  bool painting_background_in_contents_space =
      paint_info.IsPaintingBackgroundInContentsSpace();
  bool paints_hit_test_data =
      (RuntimeEnabledFeatures::HitTestOpaquenessEnabled() &&
       painting_background_in_contents_space) ||
      ObjectPainter(layout_view).ShouldRecordSpecialHitTestData(paint_info);

  Element* element = DynamicTo<Element>(layout_view.GetNode());
  bool paints_region_capture_data =
      element && element->GetRegionCaptureCropId() &&
      // TODO(wangxianzhu): This is to avoid the side-effect of
      // HitTestOpaqueness on region capture data. Verify if the side-effect
      // really matters.
      !(painting_background_in_contents_space &&
        paint_info.ShouldSkipBackground());
  bool paints_scroll_hit_test =
      !painting_background_in_contents_space &&
      layout_view.FirstFragment().PaintProperties()->Scroll();
  bool is_represented_via_pseudo_elements = [&layout_view]() {
    if (auto* transition =
            ViewTransitionUtils::GetTransition(layout_view.GetDocument())) {
      return transition->IsRepresentedViaPseudoElements(layout_view);
    }
    return false;
  }();
  if (!layout_view.HasBoxDecorationBackground() && !paints_hit_test_data &&
      !paints_scroll_hit_test && !paints_region_capture_data &&
      !is_represented_via_pseudo_elements) {
    return;
  }

  // The background rect always includes at least the visible content size.
  PhysicalRect background_rect(BackgroundRect());

  const Document& document = layout_view.GetDocument();

  // When printing or painting a preview, paint the entire unclipped scrolling
  // content area.
  if (document.IsPrintingOrPaintingPreview() ||
      !layout_view.GetFrameView()->GetFrame().ClipsContent()) {
    background_rect.Unite(layout_view.DocumentRect());
  }

  const DisplayItemClient* background_client = &layout_view;

  if (painting_background_in_contents_space) {
    // Scrollable overflow, combined with the visible content size.
    auto document_rect = layout_view.DocumentRect();
    // DocumentRect is relative to ScrollOrigin. Add ScrollOrigin to let it be
    // in the space of ContentsProperties(). See ScrollTranslation in
    // object_paint_properties.h for details.
    document_rect.Move(PhysicalOffset(layout_view.ScrollOrigin()));
    background_rect.Unite(document_rect);
    background_client = &layout_view.GetScrollableArea()
                             ->GetScrollingBackgroundDisplayItemClient();
  }

  gfx::Rect pixel_snapped_background_rect = ToPixelSnappedRect(background_rect);

  auto root_element_background_painting_state =
      layout_view.FirstFragment().ContentsProperties();

  std::optional<ScopedPaintChunkProperties> scoped_properties;

  bool painted_separate_backdrop = false;
  bool painted_separate_effect = false;

  bool should_apply_root_background_behavior =
      document.IsHTMLDocument() || document.IsXHTMLDocument();

  bool should_paint_background = !paint_info.ShouldSkipBackground() &&
                                 (layout_view.HasBoxDecorationBackground() ||
                                  is_represented_via_pseudo_elements);

  LayoutObject* root_object = nullptr;
  if (auto* document_element = document.documentElement())
    root_object = document_element->GetLayoutObject();

  // For HTML and XHTML documents, the root element may paint in a different
  // clip, effect or transform state than the LayoutView. For
  // example, the HTML element may have a clip-path, filter, blend-mode,
  // or opacity.  (However, we should ignore differences in transform.)
  //
  // In these cases, we should paint the background of the root element in
  // its LocalBorderBoxProperties() state, as part of the Root Element Group
  // [1]. In addition, for the main frame of the page, we also need to paint the
  // default backdrop color in the Root Group [2]. The Root Group paints in
  // the scrolling space of the LayoutView (i.e. its ContentsProperties()).
  //
  // [1] https://drafts.fxtf.org/compositing/#pagebackdrop
  // [2] https://drafts.fxtf.org/compositing/#rootgroup
  if (should_paint_background && painting_background_in_contents_space &&
      should_apply_root_background_behavior && root_object) {
    auto document_element_state =
        root_object->FirstFragment().LocalBorderBoxProperties();
    document_element_state.SetTransform(
        root_object->FirstFragment().PreTransform());

    // As an optimization, only paint a separate PaintChunk for the
    // root group if its property tree state differs from root element
    // group's. Otherwise we can usually avoid both a separate
    // PaintChunk and a BeginLayer/EndLayer.
    if (document_element_state != root_element_background_painting_state) {
      if (&document_element_state.Effect() !=
          &root_element_background_painting_state.Effect())
        painted_separate_effect = true;

      root_element_background_painting_state = document_element_state;
      PaintRootGroup(paint_info, pixel_snapped_background_rect, document,
                     *background_client,
                     layout_view.FirstFragment().ContentsProperties());
      painted_separate_backdrop = true;
    }
  }

  if (painting_background_in_contents_space) {
    scoped_properties.emplace(paint_info.context.GetPaintController(),
                              root_element_background_painting_state,
                              *background_client,
                              DisplayItem::kDocumentBackground);
  }

  if (should_paint_background) {
    PaintRootElementGroup(paint_info, pixel_snapped_background_rect,
                          root_element_background_painting_state,
                          *background_client, painted_separate_backdrop,
                          painted_separate_effect);
  }
  if (paints_hit_test_data) {
    ObjectPainter(layout_view)
        .RecordHitTestData(paint_info, pixel_snapped_background_rect,
                           *background_client);
  }

  if (paints_region_capture_data) {
    BoxPainter(layout_view)
        .RecordRegionCaptureData(paint_info,
                                 PhysicalRect(pixel_snapped_background_rect),
                                 *background_client);
  }

  // Record the scroll hit test after the non-scrolling background so
  // background squashing is not affected. Hit test order would be equivalent
  // if this were immediately before the non-scrolling background.
  if (paints_scroll_hit_test) {
    DCHECK(!painting_background_in_contents_space);

    // The root never fragments. In paged media page fragments are inserted
    // under the LayoutView, but the LayoutView itself never fragments.
    DCHECK(!layout_view.IsFragmented());

    BoxPainter(layout_view)
        .RecordScrollHitTestData(paint_info, *background_client,
                                 &layout_view.FirstFragment());
  }
}

// This function handles background painting for the LayoutView.
// View background painting is special in the following ways:
// 1. The view paints background for the root element, the background
//    positioning respects the positioning (but not transform) of the root
//    element. However, this method assumes that there is already a
//    PaintChunk being recorded with the LocalBorderBoxProperties of the
//    root element. Therefore the transform of the root element
//    are applied via PaintChunksToCcLayer, and not via the display list of the
//    PaintChunk itself.
// 2. CSS background-clip is ignored, the background layers always expand to
//    cover the whole canvas.
// 3. The main frame is also responsible for painting the user-agent-defined
//    base background color. Conceptually it should be painted by the embedder
//    but painting it here allows culling and pre-blending optimization when
//    possible.
void ViewPainter::PaintRootElementGroup(
    const PaintInfo& paint_info,
    const gfx::Rect& pixel_snapped_background_rect,
    const PropertyTreeStateOrAlias& background_paint_state,
    const DisplayItemClient& background_client,
    bool painted_separate_backdrop,
    bool painted_separate_effect) {
  GraphicsContext& context = paint_info.context;
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, background_client, DisplayItem::kDocumentBackground)) {
    return;
  }
  DrawingRecorder recorder(context, background_client,
                           DisplayItem::kDocumentBackground,
                           pixel_snapped_background_rect);

  const LayoutView& layout_view = GetLayoutView();
  const Document& document = layout_view.GetDocument();
  const LocalFrameView& frame_view = *layout_view.GetFrameView();
  const ComputedStyle& style = box_fragment_.Style();
  bool paints_base_background =
      frame_view.ShouldPaintBaseBackgroundColor() &&
      !frame_view.BaseBackgroundColor().IsFullyTransparent();
  Color base_background_color =
      paints_base_background ? frame_view.BaseBackgroundColor() : Color();
  if (document.Printing() && base_background_color == Color::kWhite) {
    // Leave a transparent background, assuming the paper or the PDF viewer
    // background is white by default. This allows further customization of the
    // background, e.g. in the case of https://crbug.com/498892.
    base_background_color = Color();
    paints_base_background = false;
  }

  Color root_element_background_color =
      style.VisitedDependentColor(GetCSSPropertyBackgroundColor());

  const LayoutObject* root_object =
      document.documentElement() ? document.documentElement()->GetLayoutObject()
                                 : nullptr;

  // Special handling for print economy mode.
  bool force_background_to_white =
      BoxModelObjectPainter::ShouldForceWhiteBackgroundForPrintEconomy(document,
                                                                       style);
  if (force_background_to_white) {
    // Leave a transparent background, assuming the paper or the PDF viewer
    // background is white by default. This allows further customization of the
    // background, e.g. in the case of https://crbug.com/498892.
    return;
  }

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));

  // Compute the enclosing rect of the view, in root element space.
  //
  // For background colors we can simply paint the document rect in the default
  // space. However, for background image, the root element paint offset (but
  // not transforms) apply. The strategy is to issue draw commands in the root
  // element's local space, which requires mapping the document background rect.
  bool background_renderable = true;
  gfx::Rect paint_rect = pixel_snapped_background_rect;
  // Offset for BackgroundImageGeometry to offset the image's origin. This makes
  // background tiling start at the root element's origin instead of the view.
  // This is different from the offset for painting, which is in |paint_rect|.
  PhysicalOffset background_image_offset;
  if (!root_object || !root_object->IsBox()) {
    background_renderable = false;
  } else {
    const auto& view_contents_state =
        layout_view.FirstFragment().ContentsProperties();
    if (view_contents_state != background_paint_state) {
      GeometryMapper::SourceToDestinationRect(
          view_contents_state.Transform(), background_paint_state.Transform(),
          paint_rect);
      if (paint_rect.IsEmpty())
        background_renderable = false;
      // With transforms, paint offset is encoded in paint property nodes but we
      // can use the |paint_rect|'s adjusted location as the offset from the
      // view to the root element.
      background_image_offset = PhysicalOffset(paint_rect.origin());
    } else {
      background_image_offset = -root_object->FirstFragment().PaintOffset();
    }

    if (box_fragment_.GetBoxType() == PhysicalFragment::kPageContainer) {
      // Background image origin is at the border box edge. A page container
      // fragment covers the entire page box. Add the offset to the page border
      // box fragment, to get past the margins.
      background_image_offset -= GetPageBorderBoxLink(box_fragment_).offset;
    }
  }

  bool should_clear_canvas =
      paints_base_background &&
      (document.GetSettings() &&
       document.GetSettings()->GetShouldClearDocumentBackground());

  if (!background_renderable) {
    if (!painted_separate_backdrop) {
      if (!base_background_color.IsFullyTransparent()) {
        context.FillRect(
            pixel_snapped_background_rect, base_background_color,
            auto_dark_mode,
            should_clear_canvas ? SkBlendMode::kSrc : SkBlendMode::kSrcOver);
      } else if (should_clear_canvas) {
        context.FillRect(pixel_snapped_background_rect, Color(), auto_dark_mode,
                         SkBlendMode::kClear);
      }
    }
    return;
  }

  recorder.UniteVisualRect(paint_rect);

  BoxPainterBase::FillLayerOcclusionOutputList reversed_paint_list;
  bool should_draw_background_in_separate_buffer =
      BoxModelObjectPainter(layout_view)
          .CalculateFillLayerOcclusionCulling(reversed_paint_list,
                                              style.BackgroundLayers());
  DCHECK(reversed_paint_list.size());

  if (painted_separate_effect) {
    should_draw_background_in_separate_buffer = true;
  } else {
    // If the root background color is opaque, isolation group can be skipped
    // because the canvas will be cleared by root background color.
    if (root_element_background_color.IsOpaque()) {
      should_draw_background_in_separate_buffer = false;
    }

    // We are going to clear the canvas with transparent pixels, isolation group
    // can be skipped.
    if (base_background_color.IsFullyTransparent() && should_clear_canvas) {
      should_draw_background_in_separate_buffer = false;
    }
  }

  // Only use BeginLayer if not only we should draw in a separate buffer, but
  // we also didn't paint a separate backdrop. Separate backdrops are always
  // painted when there is any effect on the root element, such as a blend
  // mode. An extra BeginLayer will result in incorrect blend isolation if
  // it is added on top of any effect on the root element.
  if (should_draw_background_in_separate_buffer && !painted_separate_effect) {
    if (!base_background_color.IsFullyTransparent()) {
      context.FillRect(
          paint_rect, base_background_color, auto_dark_mode,
          should_clear_canvas ? SkBlendMode::kSrc : SkBlendMode::kSrcOver);
    }
    context.BeginLayer();
  }

  Color combined_background_color =
      should_draw_background_in_separate_buffer
          ? root_element_background_color
          : base_background_color.Blend(root_element_background_color);

  if (combined_background_color != frame_view.BaseBackgroundColor())
    context.GetPaintController().SetFirstPainted();

  if (!combined_background_color.IsFullyTransparent()) {
    context.FillRect(
        paint_rect, combined_background_color, auto_dark_mode,
        (should_draw_background_in_separate_buffer || should_clear_canvas)
            ? SkBlendMode::kSrc
            : SkBlendMode::kSrcOver);
  } else if (should_clear_canvas &&
             !should_draw_background_in_separate_buffer) {
    context.FillRect(paint_rect, Color(), auto_dark_mode, SkBlendMode::kClear);
  }

  BoxBackgroundPaintContext bg_paint_context(layout_view, &box_fragment_,
                                             background_image_offset);
  BoxModelObjectPainter box_model_painter(layout_view);
  for (const auto* fill_layer : base::Reversed(reversed_paint_list)) {
    box_model_painter.PaintFillLayer(paint_info, Color(), *fill_layer,
                                     PhysicalRect(paint_rect),
                                     kBackgroundBleedNone, bg_paint_context);
  }

  if (should_draw_background_in_separate_buffer && !painted_separate_effect)
    context.EndLayer();
}

const LayoutView& ViewPainter::GetLayoutView() const {
  return *box_fragment_.GetLayoutObject()->View();
}

PhysicalRect ViewPainter::BackgroundRect() const {
  if (box_fragment_.GetBoxType() == PhysicalFragment::kPageContainer) {
    return box_fragment_.LocalRect();
  }
  return GetLayoutView().BackgroundRect();
}

}  // namespace blink
