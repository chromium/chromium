// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_border_edges.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/list_marker_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_fieldset_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/theme_painter.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"

namespace blink {

namespace {

LayoutRectOutsets BoxStrutToLayoutRectOutsets(
    const NGPixelSnappedPhysicalBoxStrut& box_strut) {
  return LayoutRectOutsets(
      LayoutUnit(box_strut.top), LayoutUnit(box_strut.right),
      LayoutUnit(box_strut.bottom), LayoutUnit(box_strut.left));
}

bool ShouldPaintBoxFragmentBorders(const LayoutObject& object) {
  if (!object.IsTableCell())
    return true;
  // Collapsed borders are painted by the containing table, not by each
  // individual table cell.
  return !ToLayoutTableCell(object).Table()->ShouldCollapseBorders();
}

bool FragmentVisibleToHitTestRequest(const NGPaintFragment& fragment,
                                     const HitTestRequest& request) {
  return fragment.Style().Visibility() == EVisibility::kVisible &&
         (request.IgnorePointerEventsNone() ||
          fragment.Style().PointerEvents() != EPointerEvents::kNone) &&
         !(fragment.GetNode() && fragment.GetNode()->IsInert());
}

// Hit tests inline ancestor elements of |fragment| who do not have their own
// box fragments.
// @param physical_offset Physical offset of |fragment| in the paint layer.
bool HitTestCulledInlineAncestors(HitTestResult& result,
                                  const NGPaintFragment& fragment,
                                  const NGPaintFragment* previous_sibling,
                                  const HitTestLocation& location_in_container,
                                  const LayoutPoint& physical_offset) {
  DCHECK(fragment.Parent());
  DCHECK(fragment.PhysicalFragment().IsInline());
  const NGPaintFragment& parent = *fragment.Parent();
  // To be passed as |accumulated_offset| to LayoutInline::HitTestCulledInline,
  // where it equals the physical offset of the containing block in paint layer.
  const LayoutPoint fallback_accumulated_offset =
      physical_offset - fragment.InlineOffsetToContainerBox().ToLayoutSize();
  const LayoutObject* limit_layout_object =
      parent.PhysicalFragment().IsLineBox() ? parent.Parent()->GetLayoutObject()
                                            : parent.GetLayoutObject();

  LayoutObject* current_layout_object = fragment.GetLayoutObject();
  for (LayoutObject* culled_parent = current_layout_object->Parent();
       culled_parent && culled_parent != limit_layout_object;
       culled_parent = culled_parent->Parent()) {
    // |culled_parent| is a culled inline element to be hit tested, since it's
    // "between" |fragment| and |fragment->Parent()| but doesn't have its own
    // box fragment.
    // To ensure the correct hit test ordering, |culled_parent| must be hit
    // tested only once after all of its descendants are hit tested:
    // - Shortcut: when |current_layout_object| is the only child (of
    // |culled_parent|), since it's just hit tested, we can safely hit test its
    // parent;
    // - General case: we hit test |culled_parent| only when it is not an
    // ancestor of |previous_sibling|; otherwise, |previous_sibling| has to be
    // hit tested first.
    // TODO(crbug.com/849331): It's wrong for bidi inline fragmentation. Fix it.
    const bool has_sibling = current_layout_object->PreviousSibling() ||
                             current_layout_object->NextSibling();
    if (has_sibling && previous_sibling &&
        previous_sibling->GetLayoutObject()->IsDescendantOf(culled_parent))
      break;

    if (culled_parent->IsLayoutInline() &&
        ToLayoutInline(culled_parent)
            ->HitTestCulledInline(result, location_in_container,
                                  fallback_accumulated_offset, &parent))
      return true;

    current_layout_object = culled_parent;
  }

  return false;
}

// Returns if this fragment may not be laid out by LayoutNG.
bool FragmentRequiresLegacyFallback(const NGPhysicalFragment& fragment) {
  // Fallback to LayoutObject if this is a root of NG block layout.
  // If this box is for this painter, LayoutNGBlockFlow will call this back.
  // Otherwise it calls legacy painters.
  return fragment.IsBlockFormattingContextRoot();
}

}  // anonymous namespace

NGBoxFragmentPainter::NGBoxFragmentPainter(const NGPaintFragment& box)
    : BoxPainterBase(&box.GetLayoutObject()->GetDocument(),
                     box.Style(),
                     box.GetLayoutObject()->GeneratingNode()),
      box_fragment_(box),
      border_edges_(
          NGBorderEdges::FromPhysical(box.PhysicalFragment().BorderEdges(),
                                      box.Style().GetWritingMode())) {
  DCHECK(box.PhysicalFragment().IsBox() ||
         box.PhysicalFragment().IsRenderedLegend());
}

void NGBoxFragmentPainter::Paint(const PaintInfo& paint_info) {
  ScopedPaintState paint_state(box_fragment_, paint_info);
  if (!ShouldPaint(paint_state))
    return;

  PaintInfo& info = paint_state.MutablePaintInfo();
  if (PhysicalFragment().IsAtomicInline())
    return PaintAtomicInline(info);

  LayoutPoint paint_offset = paint_state.PaintOffset();
  PaintPhase original_phase = info.phase;

  if (original_phase == PaintPhase::kOutline) {
    info.phase = PaintPhase::kDescendantOutlinesOnly;
  } else if (ShouldPaintSelfBlockBackground(original_phase)) {
    info.phase = PaintPhase::kSelfBlockBackgroundOnly;
    PaintObject(info, paint_offset);
    if (ShouldPaintDescendantBlockBackgrounds(original_phase))
      info.phase = PaintPhase::kDescendantBlockBackgroundsOnly;
  }

  if (original_phase != PaintPhase::kSelfBlockBackgroundOnly &&
      original_phase != PaintPhase::kSelfOutlineOnly) {
    if ((original_phase == PaintPhase::kForeground ||
         original_phase == PaintPhase::kFloat ||
         original_phase == PaintPhase::kDescendantOutlinesOnly) &&
        box_fragment_.GetLayoutObject()->IsBox()) {
      ScopedBoxContentsPaintState contents_paint_state(
          paint_state, ToLayoutBox(*box_fragment_.GetLayoutObject()));
      PaintObject(contents_paint_state.GetPaintInfo(),
                  contents_paint_state.PaintOffset());
    } else {
      PaintObject(info, paint_offset);
    }
  }

  if (ShouldPaintSelfOutline(original_phase)) {
    info.phase = PaintPhase::kSelfOutlineOnly;
    PaintObject(info, paint_offset);
  }

  // Our scrollbar widgets paint exactly when we tell them to, so that they work
  // properly with z-index. We paint after we painted the background/border, so
  // that the scrollbars will sit above the background/border.
  info.phase = original_phase;
  PaintOverflowControlsIfNeeded(info, paint_offset);
}

void NGBoxFragmentPainter::RecordHitTestData(const PaintInfo& paint_info,
                                             const LayoutPoint& paint_offset) {
  // Hit test display items are only needed for compositing. This flag is used
  // for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return;

  // If an object is not visible, it does not participate in hit testing.
  if (box_fragment_.Style().Visibility() != EVisibility::kVisible)
    return;

  const NGPhysicalFragment& physical_fragment = PhysicalFragment();
  auto touch_action = physical_fragment.EffectiveWhitelistedTouchAction();
  if (touch_action == TouchAction::kTouchActionAuto)
    return;

  // TODO(pdr): If we are painting the background into the scrolling contents
  // layer, we need to use the overflow rect instead of the border box rect. We
  // may want to move the call to RecordHitTestRect into
  // BoxPainter::PaintBoxDecorationBackgroundWithRect and share the logic
  // the background painting code already uses.
  NGPhysicalOffsetRect border_box = physical_fragment.LocalRect();
  if (physical_fragment.IsInline())
    border_box.offset += box_fragment_.InlineOffsetToContainerBox();
  border_box.offset += NGPhysicalOffset(paint_offset);
  HitTestData::RecordHitTestRect(
      paint_info.context, box_fragment_,
      HitTestRect(border_box.ToLayoutRect(), touch_action));
}

void NGBoxFragmentPainter::PaintObject(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    bool suppress_box_decoration_background) {
  const PaintPhase paint_phase = paint_info.phase;
  const ComputedStyle& style = box_fragment_.Style();
  bool is_visible = style.Visibility() == EVisibility::kVisible;

  if (ShouldPaintSelfBlockBackground(paint_phase)) {
    // TODO(eae): style.HasBoxDecorationBackground isn't good enough, it needs
    // to check the object as some objects may have box decoration background
    // other than from their own style.
    // TODO(eae): We can probably get rid of suppress_box_decoration_background.
    if (!suppress_box_decoration_background && is_visible)
      PaintBoxDecorationBackground(paint_info, paint_offset);

    if (RuntimeEnabledFeatures::PaintTouchActionRectsEnabled())
      RecordHitTestData(paint_info, paint_offset);

    // Record the scroll hit test after the background so background squashing
    // is not affected. Hit test order would be equivalent if this were
    // immediately before the background.
    // if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    //  PaintScrollHitTestDisplayItem(paint_info);

    // We're done. We don't bother painting any children.
    if (paint_phase == PaintPhase::kSelfBlockBackgroundOnly)
      return;
  }

  if (paint_phase == PaintPhase::kMask && is_visible)
    return PaintMask(paint_info, paint_offset);

  if (paint_phase == PaintPhase::kForeground && paint_info.IsPrinting()) {
    NGFragmentPainter(box_fragment_)
        .AddPDFURLRectIfNeeded(paint_info, paint_offset);
  }

  if (paint_phase != PaintPhase::kSelfOutlineOnly) {
    if (PhysicalFragment().ChildrenInline()) {
      if (PhysicalFragment().IsBlockFlow()) {
        PaintBlockFlowContents(paint_info, paint_offset);
        if (paint_phase == PaintPhase::kFloat ||
            paint_phase == PaintPhase::kSelection ||
            paint_phase == PaintPhase::kTextClip)
          PaintFloats(paint_info);
      } else {
        PaintInlineChildren(box_fragment_.Children(), paint_info, paint_offset);
      }
    } else {
      PaintBlockChildren(paint_info);
    }
  }

  if (ShouldPaintSelfOutline(paint_phase))
    NGFragmentPainter(box_fragment_).PaintOutline(paint_info, paint_offset);

  // If the caret's node's fragment's containing block is this block, and
  // the paint action is PaintPhaseForeground, then paint the caret.
  if (paint_phase == PaintPhase::kForeground &&
      box_fragment_.ShouldPaintCarets())
    PaintCarets(paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintCarets(const PaintInfo& paint_info,
                                       const LayoutPoint& paint_offset) {
  LocalFrame* frame = box_fragment_.GetLayoutObject()->GetFrame();

  if (box_fragment_.ShouldPaintCursorCaret())
    frame->Selection().PaintCaret(paint_info.context, paint_offset);

  if (box_fragment_.ShouldPaintDragCaret()) {
    frame->GetPage()->GetDragCaret().PaintDragCaret(frame, paint_info.context,
                                                    paint_offset);
  }
}

void NGBoxFragmentPainter::PaintBlockFlowContents(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  // Avoid painting descendants of the root element when stylesheets haven't
  // loaded. This eliminates FOUC.  It's ok not to draw, because later on, when
  // all the stylesheets do load, styleResolverMayHaveChanged() on Document will
  // trigger a full paint invalidation.
  // TODO(layout-dev): Handle without delegating to LayoutObject.
  LayoutObject* layout_object = box_fragment_.GetLayoutObject();
  if (layout_object->GetDocument().DidLayoutWithPendingStylesheets() &&
      !layout_object->IsLayoutView()) {
    return;
  }

  DCHECK(PhysicalFragment().ChildrenInline());

  LayoutRect overflow_rect(box_fragment_.ChildrenInkOverflow());
  overflow_rect.MoveBy(paint_offset);
  if (!paint_info.GetCullRect().IntersectsCullRect(overflow_rect))
    return;

  if (paint_info.phase == PaintPhase::kMask) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, box_fragment_,
            DisplayItem::PaintPhaseToDrawingType(paint_info.phase)))
      return;
    DrawingRecorder recorder(
        paint_info.context, box_fragment_,
        DisplayItem::PaintPhaseToDrawingType(paint_info.phase));
    PaintMask(paint_info, paint_offset);
    return;
  }

  PaintLineBoxChildren(box_fragment_.Children(), paint_info.ForDescendants(),
                       paint_offset);
}

void NGBoxFragmentPainter::PaintInlineChild(const NGPaintFragment& child,
                                            const PaintInfo& paint_info,
                                            const LayoutPoint& paint_offset) {
  // Atomic-inline children should be painted by PaintAtomicInlineChild.
  DCHECK(!child.PhysicalFragment().IsAtomicInline());

  const NGPhysicalFragment& fragment = child.PhysicalFragment();
  PaintInfo descendants_info = paint_info.ForDescendants();
  if (fragment.Type() == NGPhysicalFragment::kFragmentText) {
    PaintTextChild(child, descendants_info, paint_offset);
  } else if (fragment.Type() == NGPhysicalFragment::kFragmentBox) {
    if (child.HasSelfPaintingLayer())
      return;
    NGInlineBoxFragmentPainter(child).Paint(descendants_info, paint_offset);
  } else {
    NOTREACHED();
  }
}

void NGBoxFragmentPainter::PaintBlockChildren(const PaintInfo& paint_info) {
  for (const NGPaintFragment* child : box_fragment_.Children()) {
    const NGPhysicalFragment& fragment = child->PhysicalFragment();
    if (child->HasSelfPaintingLayer() || fragment.IsFloating())
      continue;

    if (fragment.Type() == NGPhysicalFragment::kFragmentBox) {
      if (FragmentRequiresLegacyFallback(fragment))
        fragment.GetLayoutObject()->Paint(paint_info);
      else
        NGBoxFragmentPainter(*child).Paint(paint_info);
    } else {
      DCHECK(fragment.Type() == NGPhysicalFragment::kFragmentRenderedLegend)
          << fragment.ToString();
    }
  }
}

void NGBoxFragmentPainter::PaintFloatingChildren(
    NGPaintFragment::ChildList children,
    const PaintInfo& paint_info) {
  for (const NGPaintFragment* child : children) {
    const NGPhysicalFragment& fragment = child->PhysicalFragment();
    if (child->HasSelfPaintingLayer())
      continue;
    if (fragment.IsFloating()) {
      // TODO(kojii): The float is outside of the inline formatting context and
      // that it maybe another NG inline formatting context, NG block layout, or
      // legacy. NGBoxFragmentPainter can handle only the first case. In order
      // to cover more tests for other two cases, we always fallback to legacy,
      // which will forward back to NGBoxFragmentPainter if the float is for
      // NGBoxFragmentPainter. We can shortcut this for the first case when
      // we're more stable.
      ObjectPainter(*child->GetLayoutObject())
          .PaintAllPhasesAtomically(paint_info);
    } else {
      PaintFloatingChildren(child->Children(), paint_info);
    }
  }
}

void NGBoxFragmentPainter::PaintFloats(const PaintInfo& paint_info) {
  // TODO(eae): The legacy paint code currently handles most floats, if they can
  // be painted by PaintNG BlockFlowPainter::PaintFloats will then call
  // NGBlockFlowPainter::Paint on each float.
  // This code is currently only used for floats within a block within inline
  // children.
  PaintInfo float_paint_info(paint_info);
  if (paint_info.phase == PaintPhase::kFloat)
    float_paint_info.phase = PaintPhase::kForeground;
  PaintFloatingChildren(box_fragment_.Children(), float_paint_info);
}

void NGBoxFragmentPainter::PaintMask(const PaintInfo& paint_info,
                                     const LayoutPoint& paint_offset) {
  DCHECK_EQ(PaintPhase::kMask, paint_info.phase);
  const ComputedStyle& style = box_fragment_.Style();
  if (!style.HasMask() || style.Visibility() != EVisibility::kVisible)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, box_fragment_, paint_info.phase))
    return;

  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(*static_cast<const LayoutBoxModelObject*>(
      box_fragment_.GetLayoutObject()));

  DrawingRecorder recorder(paint_info.context, box_fragment_, paint_info.phase);
  LayoutRect paint_rect =
      LayoutRect(paint_offset, box_fragment_.Size().ToLayoutSize());
  PaintMaskImages(paint_info, paint_rect, box_fragment_, geometry,
                  border_edges_.line_left, border_edges_.line_right);
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  if (box_fragment_.PhysicalFragment().IsFieldsetContainer()) {
    NGFieldsetPainter(box_fragment_)
        .PaintBoxDecorationBackground(paint_info, paint_offset);
    return;
  }

  // Note that for fieldsets we need to enter decoration and background painting
  // even if we have no such things, because the rendered legend is painted in
  // this phase as well. Hence the early check above.
  const ComputedStyle& style = box_fragment_.Style();
  if (!style.HasBoxDecorationBackground())
    return;

  // TODO(mstensho): Break dependency on LayoutObject functionality.
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();

  LayoutRect paint_rect;
  base::Optional<ScopedBoxContentsPaintState> contents_paint_state;
  if (IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          box_fragment_, paint_info)) {
    // For the case where we are painting the background into the scrolling
    // contents layer of a composited scroller we need to include the entire
    // overflow rect.
    const LayoutBox& layout_box = ToLayoutBox(layout_object);
    paint_rect = layout_box.PhysicalLayoutOverflowRect();

    contents_paint_state.emplace(paint_info, paint_offset, layout_box);
    paint_rect.MoveBy(contents_paint_state->PaintOffset());

    // The background painting code assumes that the borders are part of the
    // paintRect so we expand the paintRect by the border size when painting the
    // background into the scrolling contents layer.
    paint_rect.Expand(layout_box.BorderBoxOutsets());
  } else {
    // TODO(eae): We need better converters for ng geometry types. Long term we
    // probably want to change the paint code to take NGPhysical* but that is a
    // much bigger change.
    NGPhysicalSize size = box_fragment_.Size();
    paint_rect = LayoutRect(LayoutPoint(), LayoutSize(size.width, size.height));
    paint_rect.MoveBy(paint_offset);
  }

  PaintBoxDecorationBackgroundWithRect(
      contents_paint_state ? contents_paint_state->GetPaintInfo() : paint_info,
      paint_rect);
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
bool NGBoxFragmentPainter::BackgroundIsKnownToBeOpaque(
    const PaintInfo& paint_info) {
  const LayoutBox& layout_box = ToLayoutBox(*box_fragment_.GetLayoutObject());
  LayoutRect bounds =
      IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          box_fragment_, paint_info)
          ? layout_box.LayoutOverflowRect()
          : layout_box.SelfVisualOverflowRect();
  return layout_box.BackgroundIsKnownToBeOpaqueInRect(bounds);
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const LayoutRect& paint_rect) {
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();
  const LayoutBox& layout_box = ToLayoutBox(layout_object);

  bool painting_overflow_contents =
      IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          box_fragment_, paint_info);
  const ComputedStyle& style = box_fragment_.Style();

  base::Optional<DisplayItemCacheSkipper> cache_skipper;
  // Disable cache in under-invalidation checking mode for MediaSliderPart
  // because we always paint using the latest data (buffered ranges, current
  // time and duration) which may be different from the cached data, and for
  // delayed-invalidation object because it may change before it's actually
  // invalidated. Note that we still report harmless under-invalidation of
  // non-delayed-invalidation animated background, which should be ignored.
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      (style.Appearance() == kMediaSliderPart ||
       layout_box.ShouldDelayFullPaintInvalidation())) {
    cache_skipper.emplace(paint_info.context);
  }

  const DisplayItemClient& display_item_client =
      painting_overflow_contents
          ? layout_box.GetScrollableArea()
                ->GetScrollingBackgroundDisplayItemClient()
          : box_fragment_;
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           DisplayItem::kBoxDecorationBackground);
  BoxDecorationData box_decoration_data(PhysicalFragment());
  GraphicsContextStateSaver state_saver(paint_info.context, false);

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      LayoutRect(EnclosingIntRect(paint_rect)) == paint_rect &&
      BackgroundIsKnownToBeOpaque(paint_info))
    recorder.SetKnownToBeOpaque();

  bool needs_end_layer = false;
  if (!painting_overflow_contents) {
    PaintNormalBoxShadow(paint_info, paint_rect, style, border_edges_.line_left,
                         border_edges_.line_right);

    if (box_fragment_.HasSelfPaintingLayer() && layout_box.IsTableCell() &&
        ToLayoutTableCell(layout_box).Table()->ShouldCollapseBorders()) {
      // We have to clip here because the background would paint on top of the
      // collapsed table borders otherwise, since this is a self-painting layer.
      LayoutRect clip_rect = paint_rect;
      clip_rect.Expand(ToLayoutTableCell(layout_box).BorderInsets());
      state_saver.Save();
      paint_info.context.Clip(PixelSnappedIntRect(clip_rect));
    } else if (BleedAvoidanceIsClipping(box_decoration_data.bleed_avoidance)) {
      state_saver.Save();
      FloatRoundedRect border = style.GetRoundedBorderFor(
          paint_rect, border_edges_.line_left, border_edges_.line_right);
      paint_info.context.ClipRoundedRect(border);

      if (box_decoration_data.bleed_avoidance == kBackgroundBleedClipLayer) {
        paint_info.context.BeginLayer();
        needs_end_layer = true;
      }
    }
  }

  IntRect snapped_paint_rect(PixelSnappedIntRect(paint_rect));
  ThemePainter& theme_painter = LayoutTheme::GetTheme().Painter();
  bool theme_painted =
      box_decoration_data.has_appearance &&
      !theme_painter.Paint(layout_box, paint_info, snapped_paint_rect);
  bool should_paint_background =
      !theme_painted && (!paint_info.SkipRootBackground() ||
                         paint_info.PaintContainer() != layout_box);
  if (should_paint_background) {
    PaintBackground(paint_info, paint_rect,
                    box_decoration_data.background_color,
                    box_decoration_data.bleed_avoidance);

    if (box_decoration_data.has_appearance) {
      theme_painter.PaintDecorations(layout_box.GetNode(),
                                     layout_box.GetDocument(), style,
                                     paint_info, snapped_paint_rect);
    }
  }

  if (!painting_overflow_contents) {
    PaintInsetBoxShadowWithBorderRect(paint_info, paint_rect, style,
                                      border_edges_.line_left,
                                      border_edges_.line_right);

    // The theme will tell us whether or not we should also paint the CSS
    // border.
    if (box_decoration_data.has_border_decoration &&
        (!box_decoration_data.has_appearance ||
         (!theme_painted &&
          LayoutTheme::GetTheme().Painter().PaintBorderOnly(
              layout_box.GetNode(), style, paint_info, snapped_paint_rect))) &&
        ShouldPaintBoxFragmentBorders(layout_object)) {
      Node* generating_node = layout_object.GeneratingNode();
      const Document& document = layout_object.GetDocument();
      PaintBorder(box_fragment_, document, generating_node, paint_info,
                  paint_rect, style, box_decoration_data.bleed_avoidance,
                  border_edges_.line_left, border_edges_.line_right);
    }
  }

  if (needs_end_layer)
    paint_info.context.EndLayer();
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void NGBoxFragmentPainter::PaintBackground(
    const PaintInfo& paint_info,
    const LayoutRect& paint_rect,
    const Color& background_color,
    BackgroundBleedAvoidance bleed_avoidance) {
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();
  const LayoutBox& layout_box = ToLayoutBox(layout_object);
  if (layout_box.BackgroundTransfersToView())
    return;
  if (layout_box.BackgroundIsKnownToBeObscured())
    return;
  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(*static_cast<const LayoutBoxModelObject*>(
      box_fragment_.GetLayoutObject()));
  PaintFillLayers(paint_info, background_color,
                  box_fragment_.Style().BackgroundLayers(), paint_rect,
                  geometry, bleed_avoidance);
}

void NGBoxFragmentPainter::PaintInlineChildBoxUsingLegacyFallback(
    const NGPhysicalFragment& fragment,
    const PaintInfo& paint_info) {
  LayoutObject* child_layout_object = fragment.GetLayoutObject();
  DCHECK(child_layout_object);
  if (child_layout_object->IsLayoutNGMixin() &&
      ToLayoutBlockFlow(child_layout_object)->PaintFragment()) {
    // This object will use NGBoxFragmentPainter.
    child_layout_object->Paint(paint_info);
    return;
  }

  if (child_layout_object->IsAtomicInlineLevel()) {
    // Pre-NG painters also expect callers to use |PaintAllPhasesAtomically()|
    // for atomic inlines.
    ObjectPainter(*child_layout_object).PaintAllPhasesAtomically(paint_info);
    return;
  }

  child_layout_object->Paint(paint_info);
}

void NGBoxFragmentPainter::PaintAllPhasesAtomically(
    const PaintInfo& paint_info,
    bool is_self_painting) {
  ScopedPaintState paint_state(box_fragment_, paint_info);
  auto paint_offset = paint_state.PaintOffset();
  PaintInfo& local_paint_info = paint_state.MutablePaintInfo();

  // Pass PaintPhaseSelection and PaintPhaseTextClip is handled by the regular
  // foreground paint implementation. We don't need complete painting for these
  // phases.
  PaintPhase phase = paint_info.phase;
  if (phase == PaintPhase::kSelection || phase == PaintPhase::kTextClip)
    return PaintObject(local_paint_info, paint_offset);

  if (paint_info.phase == PaintPhase::kSelfBlockBackgroundOnly &&
      is_self_painting) {
    PaintObject(local_paint_info, paint_offset);
    PaintOverflowControlsIfNeeded(local_paint_info, paint_offset);
    return;
  }

  if (phase != PaintPhase::kForeground)
    return;

  if (!is_self_painting) {
    local_paint_info.phase = PaintPhase::kBlockBackground;
    PaintObject(local_paint_info, paint_offset);
  }
  local_paint_info.phase = PaintPhase::kFloat;
  PaintObject(local_paint_info, paint_offset);

  local_paint_info.phase = PaintPhase::kForeground;
  if (box_fragment_.GetLayoutObject()->IsBox()) {
    ScopedBoxContentsPaintState contents_paint_state(
        paint_state, ToLayoutBox(*box_fragment_.GetLayoutObject()));
    PaintObject(contents_paint_state.GetPaintInfo(),
                contents_paint_state.PaintOffset());
  } else {
    PaintObject(local_paint_info, paint_offset);
  }

  local_paint_info.phase = PaintPhase::kOutline;
  PaintObject(local_paint_info, paint_offset);

  if (!is_self_painting) {
    local_paint_info.phase = PaintPhase::kBlockBackground;
    PaintOverflowControlsIfNeeded(local_paint_info, paint_offset);
  }
}

void NGBoxFragmentPainter::PaintLineBoxChildren(
    NGPaintFragment::ChildList line_boxes,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  // Only paint during the foreground/selection phases.
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kTextClip &&
      paint_info.phase != PaintPhase::kMask &&
      paint_info.phase != PaintPhase::kDescendantOutlinesOnly &&
      paint_info.phase != PaintPhase::kOutline)
    return;

  // The only way an inline could paint like this is if it has a layer.
  const auto* layout_object = box_fragment_.GetLayoutObject();
  DCHECK(layout_object->IsLayoutBlock() ||
         (layout_object->IsLayoutInline() && layout_object->HasLayer()));

  // if (paint_info.phase == PaintPhase::kForeground && paint_info.IsPrinting())
  //  AddPDFURLRectsForInlineChildrenRecursively(layout_object, paint_info,
  //                                             paint_offset);

  // If we have no lines then we have no work to do.
  if (!line_boxes.size())
    return;

  // TODO(layout-dev): Early return if no line intersects cull rect.
  for (const NGPaintFragment* line : line_boxes) {
    if (line->PhysicalFragment().IsFloatingOrOutOfFlowPositioned())
      continue;
    const LayoutPoint child_offset =
        paint_offset + line->Offset().ToLayoutPoint();
    if (line->PhysicalFragment().IsListMarker()) {
      PaintAtomicInlineChild(*line, paint_info);
      continue;
    }
    DCHECK(line->PhysicalFragment().IsLineBox())
        << line->PhysicalFragment().ToString();
    PaintInlineChildren(line->Children(), paint_info, child_offset);
  }
}

void NGBoxFragmentPainter::PaintInlineChildren(
    NGPaintFragment::ChildList inline_children,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  for (const NGPaintFragment* child : inline_children) {
    if (child->PhysicalFragment().IsFloating())
      continue;
    if (child->PhysicalFragment().IsAtomicInline()) {
      PaintAtomicInlineChild(*child, paint_info);
    } else {
      PaintInlineChild(*child, paint_info, paint_offset);
    }
  }
}

void NGBoxFragmentPainter::PaintInlineChildrenOutlines(
    NGPaintFragment::ChildList line_boxes,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  // TODO(layout-dev): Implement.
}

void NGBoxFragmentPainter::PaintAtomicInlineChild(const NGPaintFragment& child,
                                                  const PaintInfo& paint_info) {
  // Inline children should be painted by PaintInlineChild.
  DCHECK(child.PhysicalFragment().IsAtomicInline());

  const NGPhysicalFragment& fragment = child.PhysicalFragment();
  if (child.HasSelfPaintingLayer())
    return;
  if (fragment.Type() == NGPhysicalFragment::kFragmentBox &&
      FragmentRequiresLegacyFallback(fragment)) {
    PaintInlineChildBoxUsingLegacyFallback(fragment, paint_info);
  } else {
    NGBoxFragmentPainter(child).PaintAllPhasesAtomically(paint_info, false);
  }
}

void NGBoxFragmentPainter::PaintTextChild(const NGPaintFragment& text_fragment,
                                          const PaintInfo& paint_info,
                                          const LayoutPoint& paint_offset) {
  // Inline blocks should be painted by PaintAtomicInlineChild.
  DCHECK(!text_fragment.PhysicalFragment().IsAtomicInline());

  // Only paint during the foreground/selection phases.
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kTextClip &&
      paint_info.phase != PaintPhase::kMask)
    return;

  // The text clip phase already has a DrawingRecorder. Text clips are initiated
  // only in BoxPainterBase::PaintFillLayer, which is already within a
  // DrawingRecorder.
  base::Optional<DrawingRecorder> recorder;
  if (paint_info.phase != PaintPhase::kTextClip) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, text_fragment,
            DisplayItem::PaintPhaseToDrawingType(paint_info.phase)))
      return;
    recorder.emplace(paint_info.context, text_fragment,
                     DisplayItem::PaintPhaseToDrawingType(paint_info.phase));
  }

  const NGPhysicalTextFragment& physical_text_fragment =
      ToNGPhysicalTextFragment(text_fragment.PhysicalFragment());
  if (physical_text_fragment.TextType() ==
      NGPhysicalTextFragment::kSymbolMarker) {
    // The NGInlineItem of marker might be Split(). So PaintSymbol only if the
    // StartOffset is 0, or it might be painted several times.
    if (!physical_text_fragment.StartOffset())
      PaintSymbol(text_fragment, paint_info, paint_offset);
  } else {
    NGTextFragmentPainter text_painter(text_fragment);
    text_painter.Paint(paint_info, paint_offset);
  }
}

void NGBoxFragmentPainter::PaintSymbol(const NGPaintFragment& fragment,
                                       const PaintInfo& paint_info,
                                       const LayoutPoint& paint_offset) {
  const ComputedStyle& style = fragment.Style();
  LayoutRect marker_rect =
      LayoutListMarker::RelativeSymbolMarkerRect(style, fragment.Size().width);
  marker_rect.MoveBy(fragment.Offset().ToLayoutPoint());
  marker_rect.MoveBy(paint_offset);
  IntRect rect = PixelSnappedIntRect(marker_rect);

  ListMarkerPainter::PaintSymbol(paint_info, fragment.GetLayoutObject(), style,
                                 rect);
}

// Follows BlockPainter::PaintInlineBox
void NGBoxFragmentPainter::PaintAtomicInline(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kSelfBlockBackgroundOnly)
    return;

  // Text clips are painted only for the direct inline children of the object
  // that has a text clip style on it, not block children.
  DCHECK(paint_info.phase != PaintPhase::kTextClip);

  bool is_self_painting = PhysicalFragment().Layer() &&
                          PhysicalFragment().Layer()->IsSelfPaintingLayer();
  PaintAllPhasesAtomically(paint_info, is_self_painting);
}

bool NGBoxFragmentPainter::
    IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
        const NGPaintFragment& fragment,
        const PaintInfo& paint_info) {
  // TODO(layout-dev): Change paint_info.PaintContainer to accept fragments
  // once LayoutNG supports scrolling containers.
  return paint_info.PaintFlags() & kPaintLayerPaintingOverflowContents &&
         !(paint_info.PaintFlags() &
           kPaintLayerPaintingCompositingBackgroundPhase) &&
         box_fragment_.GetLayoutObject() == paint_info.PaintContainer();
}

// Clone of BlockPainter::PaintOverflowControlsIfNeeded
void NGBoxFragmentPainter::PaintOverflowControlsIfNeeded(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  if (box_fragment_.HasOverflowClip() &&
      box_fragment_.Style().Visibility() == EVisibility::kVisible &&
      ShouldPaintSelfBlockBackground(paint_info.phase)) {
    ScrollableAreaPainter(*PhysicalFragment().Layer()->GetScrollableArea())
        .PaintOverflowControls(paint_info, RoundedIntPoint(paint_offset),
                               false /* painting_overlay_controls */);
  }
}

bool NGBoxFragmentPainter::ShouldPaint(
    const ScopedPaintState& paint_state) const {
  // TODO(layout-dev): Add support for scrolling, see BlockPainter::ShouldPaint.
  return paint_state.LocalRectIntersectsCullRect(
      box_fragment_.SelfInkOverflow());
}

void NGBoxFragmentPainter::PaintTextClipMask(GraphicsContext& context,
                                             const IntRect& mask_rect,
                                             const LayoutPoint& paint_offset,
                                             bool object_has_multiple_boxes) {
  PaintInfo paint_info(context, mask_rect, PaintPhase::kTextClip,
                       kGlobalPaintNormalPhase, 0);
  if (object_has_multiple_boxes) {
    LayoutSize local_offset = box_fragment_.Offset().ToLayoutSize();
    NGInlineBoxFragmentPainter inline_box_painter(box_fragment_);
    if (box_fragment_.Style().BoxDecorationBreak() ==
        EBoxDecorationBreak::kSlice) {
      LayoutUnit offset_on_line;
      LayoutUnit total_width;
      inline_box_painter.ComputeFragmentOffsetOnLine(
          box_fragment_.Style().Direction(), &offset_on_line, &total_width);
      LayoutSize line_offset(offset_on_line, LayoutUnit());
      local_offset -= box_fragment_.Style().IsHorizontalWritingMode()
                          ? line_offset
                          : line_offset.TransposedSize();
    }
    inline_box_painter.Paint(paint_info, paint_offset - local_offset);
  } else {
    PaintObject(paint_info, paint_offset);
  }
}

LayoutRect NGBoxFragmentPainter::AdjustRectForScrolledContent(
    const PaintInfo& paint_info,
    const BoxPainterBase::FillLayerInfo& info,
    const LayoutRect& rect) {
  LayoutRect scrolled_paint_rect = rect;
  GraphicsContext& context = paint_info.context;
  const NGPhysicalBoxFragment& physical = PhysicalFragment();

  // Clip to the overflow area.
  if (info.is_clipped_with_local_scrolling &&
      !IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          box_fragment_, paint_info)) {
    context.Clip(FloatRect(physical.OverflowClipRect(rect.Location())));

    // Adjust the paint rect to reflect a scrolled content box with borders at
    // the ends.
    IntSize offset = physical.ScrolledContentOffset();
    scrolled_paint_rect.Move(-offset);
    LayoutRectOutsets borders = AdjustedBorderOutsets(info);
    scrolled_paint_rect.SetSize(physical.ScrollSize() + borders.Size());
  }
  return scrolled_paint_rect;
}

LayoutRectOutsets NGBoxFragmentPainter::ComputeBorders() const {
  return BoxStrutToLayoutRectOutsets(
      box_fragment_.PhysicalFragment().BorderWidths());
}

LayoutRectOutsets NGBoxFragmentPainter::ComputePadding() const {
  return BoxStrutToLayoutRectOutsets(
      ToNGPhysicalBoxFragment(box_fragment_.PhysicalFragment())
          .PixelSnappedPadding());
}

BoxPainterBase::FillLayerInfo NGBoxFragmentPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance) const {
  return BoxPainterBase::FillLayerInfo(
      box_fragment_.GetLayoutObject()->GetDocument(), box_fragment_.Style(),
      box_fragment_.HasOverflowClip(), color, bg_layer, bleed_avoidance,
      border_edges_.line_left, border_edges_.line_right);
}

bool NGBoxFragmentPainter::IsInSelfHitTestingPhase(HitTestAction action) const {
  // TODO(layout-dev): We should set an IsContainingBlock flag on
  // NGPhysicalBoxFragment, instead of routing back to LayoutObject.
  const LayoutObject* layout_object = box_fragment_.GetLayoutObject();
  if (layout_object->IsBox())
    return ToLayoutBox(layout_object)->IsInSelfHitTestingPhase(action);
  return action == kHitTestForeground;
}

bool NGBoxFragmentPainter::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& physical_offset,
    HitTestAction action) {
  // TODO(eae): Switch to using NG geometry types.
  LayoutSize size(box_fragment_.Size().width, box_fragment_.Size().height);
  const ComputedStyle& style = box_fragment_.Style();

  bool hit_test_self = IsInSelfHitTestingPhase(action);

  // TODO(layout-dev): Add support for hit testing overflow controls once we
  // overflow has been implemented.
  // if (hit_test_self && HasOverflowClip() &&
  //   HitTestOverflowControl(result, location_in_container, physical_offset))
  // return true;

  bool skip_children = false;
  if (box_fragment_.ShouldClipOverflow()) {
    // PaintLayer::HitTestContentsForFragments checked the fragments'
    // foreground rect for intersection if a layer is self painting,
    // so only do the overflow clip check here for non-self-painting layers.
    if (!box_fragment_.HasSelfPaintingLayer() &&
        !location_in_container.Intersects(PhysicalFragment().OverflowClipRect(
            physical_offset, kExcludeOverlayScrollbarSizeForHitTesting))) {
      skip_children = true;
    }
    if (!skip_children && style.HasBorderRadius()) {
      LayoutRect bounds_rect(physical_offset, size);
      skip_children = !location_in_container.Intersects(
          style.GetRoundedInnerBorderFor(bounds_rect));
    }
  }

  if (!skip_children) {
    const IntSize scrolled_offset =
        box_fragment_.HasOverflowClip()
            ? PhysicalFragment().ScrolledContentOffset()
            : IntSize();
    if (HitTestChildren(result, box_fragment_.Children(), location_in_container,
                        physical_offset - scrolled_offset, action)) {
      return true;
    }
  }

  if (style.HasBorderRadius() &&
      HitTestClippedOutByBorder(location_in_container, physical_offset))
    return false;

  // Now hit test ourselves.
  if (hit_test_self && VisibleToHitTestRequest(result.GetHitTestRequest())) {
    LayoutRect bounds_rect(physical_offset, size);
    if (location_in_container.Intersects(bounds_rect)) {
      Node* node = box_fragment_.NodeForHitTest();
      if (!result.InnerNode() && node) {
        LayoutPoint point =
            location_in_container.Point() - ToLayoutSize(physical_offset);
        result.SetNodeAndPosition(node, point);
      }
      if (result.AddNodeToListBasedTestResult(node, location_in_container,
                                              bounds_rect) == kStopHitTesting) {
        return true;
      }
    }
  }

  return false;
}

bool NGBoxFragmentPainter::VisibleToHitTestRequest(
    const HitTestRequest& request) const {
  return FragmentVisibleToHitTestRequest(box_fragment_, request);
}

bool NGBoxFragmentPainter::HitTestTextFragment(
    HitTestResult& result,
    const NGPaintFragment& text_paint_fragment,
    const HitTestLocation& location_in_container,
    const LayoutPoint& physical_offset,
    HitTestAction action) {
  if (action != kHitTestForeground)
    return false;

  const NGPhysicalFragment& text_fragment =
      text_paint_fragment.PhysicalFragment();
  LayoutSize size(text_fragment.Size().width, text_fragment.Size().height);
  LayoutRect border_rect(physical_offset, size);
  const ComputedStyle& style = text_fragment.Style();

  if (style.HasBorderRadius()) {
    FloatRoundedRect border = style.GetRoundedBorderFor(
        border_rect,
        text_fragment.BorderEdges() & NGBorderEdges::Physical::kLeft,
        text_fragment.BorderEdges() & NGBorderEdges::Physical::kRight);
    if (!location_in_container.Intersects(border))
      return false;
  }

  // TODO(layout-dev): Clip to line-top/bottom.
  LayoutRect rect = LayoutRect(PixelSnappedIntRect(border_rect));
  if (FragmentVisibleToHitTestRequest(text_paint_fragment,
                                      result.GetHitTestRequest()) &&
      location_in_container.Intersects(rect)) {
    Node* node = text_paint_fragment.NodeForHitTest();
    if (!result.InnerNode() && node) {
      LayoutPoint point =
          location_in_container.Point() - ToLayoutSize(physical_offset) +
          text_paint_fragment.InlineOffsetToContainerBox().ToLayoutPoint();
      result.SetNodeAndPosition(node, point);
    }

    if (result.AddNodeToListBasedTestResult(node, location_in_container,
                                            rect) == kStopHitTesting) {
      return true;
    }
  }

  return false;
}

// Replicates logic in legacy InlineFlowBox::NodeAtPoint().
bool NGBoxFragmentPainter::HitTestLineBoxFragment(
    HitTestResult& result,
    const NGPaintFragment& fragment,
    const HitTestLocation& location_in_container,
    const LayoutPoint& physical_offset,
    HitTestAction action) {
  if (HitTestChildren(result, fragment.Children(), location_in_container,
                      physical_offset, action))
    return true;

  if (action != kHitTestForeground)
    return false;

  if (!VisibleToHitTestRequest(result.GetHitTestRequest()))
    return false;

  const LayoutPoint overflow_location =
      fragment.SelfInkOverflow().Location() + physical_offset;
  if (HitTestClippedOutByBorder(location_in_container, overflow_location))
    return false;

  const LayoutSize size = fragment.Size().ToLayoutSize();
  const LayoutRect bounds_rect(physical_offset, size);
  const ComputedStyle& containing_box_style = box_fragment_.Style();
  if (containing_box_style.HasBorderRadius() &&
      !location_in_container.Intersects(
          containing_box_style.GetRoundedBorderFor(bounds_rect))) {
    return false;
  }

  // Now hit test ourselves.
  if (!location_in_container.Intersects(bounds_rect))
    return false;

  Node* node = fragment.NodeForHitTest();
  if (!result.InnerNode() && node) {
    const LayoutPoint point =
        location_in_container.Point() - ToLayoutSize(physical_offset) +
        fragment.InlineOffsetToContainerBox().ToLayoutPoint();
    result.SetNodeAndPosition(node, point);
  }
  return result.AddNodeToListBasedTestResult(node, location_in_container,
                                             bounds_rect) == kStopHitTesting;
}

bool NGBoxFragmentPainter::HitTestChildBoxFragment(
    HitTestResult& result,
    const NGPaintFragment& paint_fragment,
    const HitTestLocation& location_in_container,
    const LayoutPoint& physical_offset,
    HitTestAction action) {
  const NGPhysicalFragment& fragment = paint_fragment.PhysicalFragment();
  if (fragment.IsFloating() && action != kHitTestFloat)
    return false;
  // Lines and inlines are hit tested only in the foreground phase.
  if (fragment.IsInline() && action != kHitTestForeground)
    return false;

  if (!FragmentRequiresLegacyFallback(fragment)) {
    // TODO(layout-dev): Implement HitTestAllPhases in NG after we stop
    // falling back to legacy for child atomic inlines and floats.
    DCHECK(!fragment.IsAtomicInline());
    DCHECK(!fragment.IsFloating());
    return NGBoxFragmentPainter(paint_fragment)
        .NodeAtPoint(result, location_in_container, physical_offset, action);
  }

  LayoutBox* const layout_box = ToLayoutBox(fragment.GetLayoutObject());

  // To be passed as |accumulated_offset| to legacy hit test functions of
  // LayoutBox or subclass overrides, where it isn't in any well-defined
  // coordinate space, but only equals the difference below.
  const LayoutPoint fallback_accumulated_offset =
      physical_offset - ToLayoutSize(layout_box->Location());

  // https://www.w3.org/TR/CSS22/zindex.html#painting-order
  // Hit test all phases of inline blocks, inline tables, replaced elements and
  // non-positioned floats as if they created their own stacking contexts.
  const bool should_hit_test_all_phases =
      fragment.IsAtomicInline() || fragment.IsFloating();
  return should_hit_test_all_phases
             ? layout_box->HitTestAllPhases(result, location_in_container,
                                            fallback_accumulated_offset)
             : layout_box->NodeAtPoint(result, location_in_container,
                                       fallback_accumulated_offset, action);
}

bool NGBoxFragmentPainter::HitTestChildren(
    HitTestResult& result,
    NGPaintFragment::ChildList children,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  Vector<NGPaintFragment*, 16> child_vector;
  children.ToList(&child_vector);
  for (unsigned i = child_vector.size(); i;) {
    const NGPaintFragment* child = child_vector[--i];
    const NGPhysicalOffset offset = child->Offset();
    if (child->HasSelfPaintingLayer())
      continue;

    const NGPhysicalFragment& fragment = child->PhysicalFragment();
    const LayoutPoint child_physical_offset =
        accumulated_offset + offset.ToLayoutPoint();

    bool stop_hit_testing = false;
    if (fragment.Type() == NGPhysicalFragment::kFragmentBox) {
      stop_hit_testing = HitTestChildBoxFragment(
          result, *child, location_in_container, child_physical_offset, action);

    } else if (fragment.Type() == NGPhysicalFragment::kFragmentLineBox) {
      stop_hit_testing = HitTestLineBoxFragment(
          result, *child, location_in_container, child_physical_offset, action);

    } else if (fragment.Type() == NGPhysicalFragment::kFragmentText) {
      // TODO(eae): Should this hit test on the text itself or the containing
      // node?
      stop_hit_testing = HitTestTextFragment(
          result, *child, location_in_container, child_physical_offset, action);
    }
    if (stop_hit_testing)
      return true;

    if (!fragment.IsInline() || action != kHitTestForeground)
      continue;

    // Hit test culled inline boxes between |fragment| and its parent fragment.
    const NGPaintFragment* previous_sibling = i ? child_vector[i - 1] : nullptr;
    if (HitTestCulledInlineAncestors(result, *child, previous_sibling,
                                     location_in_container,
                                     child_physical_offset))
      return true;
  }

  return false;
}

bool NGBoxFragmentPainter::HitTestClippedOutByBorder(
    const HitTestLocation& location_in_container,
    const LayoutPoint& border_box_location) const {
  const ComputedStyle& style = box_fragment_.Style();
  LayoutRect rect =
      LayoutRect(LayoutPoint(), PhysicalFragment().Size().ToLayoutSize());
  rect.MoveBy(border_box_location);
  return !location_in_container.Intersects(style.GetRoundedBorderFor(
      rect, border_edges_.line_left, border_edges_.line_right));
}

const NGPhysicalBoxFragment& NGBoxFragmentPainter::PhysicalFragment() const {
  return static_cast<const NGPhysicalBoxFragment&>(
      box_fragment_.PhysicalFragment());
}

}  // namespace blink
