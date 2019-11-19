// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/line_box_list_painter.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_box_model.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/line/inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/line_box_list.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

namespace {

// Recursively build up backplates behind inline text boxes, each split at the
// paragraph level. Store the results in paragraph_backplates.
void BuildBackplate(const InlineFlowBox* box,
                    const PhysicalOffset& paint_offset,
                    PhysicalRect* current_backplate,
                    int* consecutive_line_breaks,
                    Vector<PhysicalRect>* paragraph_backplates) {
  DCHECK(current_backplate && consecutive_line_breaks && paragraph_backplates);

  // The number of consecutive forced breaks that split the backplate by
  // paragraph.
  static constexpr int kMaxConsecutiveLineBreaks = 2;

  // Build up and paint backplates of all child inline text boxes. We are not
  // able to simply use the linebox rect to compute the backplate because the
  // backplate should only be painted for inline text and not for atomic
  // inlines.
  for (InlineBox* child = box->FirstChild(); child;
       child = child->NextOnLine()) {
    LineLayoutItem layout_item = child->GetLineLayoutItem();
    if (layout_item.IsText() || layout_item.IsListMarker()) {
      if (layout_item.IsText()) {
        String child_text =
            ToInlineTextBox(child)->GetLineLayoutItem().GetText();
        if (ToInlineTextBox(child)->IsLineBreak() ||
            child_text.StartsWith('\n'))
          (*consecutive_line_breaks)++;
      }
      if (*consecutive_line_breaks >= kMaxConsecutiveLineBreaks) {
        // This is a paragraph point.
        paragraph_backplates->push_back(*current_backplate);
        *current_backplate = PhysicalRect();
        *consecutive_line_breaks = 0;
      }

      PhysicalOffset box_origin(PhysicalOffset(child->Location()) +
                                paint_offset);
      PhysicalRect box_rect(box_origin, PhysicalSize(child->LogicalWidth(),
                                                     child->LogicalHeight()));
      if (*consecutive_line_breaks > 0 && !box_rect.IsEmpty()) {
        // Text was reached, so reset consecutive_line_breaks.
        *consecutive_line_breaks = 0;
      }
      current_backplate->Unite(box_rect);
    } else if (child->IsInlineFlowBox()) {
      // If an inline flow box was reached, continue to recursively build up the
      // backplate.
      BuildBackplate(ToInlineFlowBox(child), paint_offset, current_backplate,
                     consecutive_line_breaks, paragraph_backplates);
    }
  }
}

}  // anonymous namespace

static void AddURLRectsForInlineChildrenRecursively(
    const LayoutObject& layout_object,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  for (LayoutObject* child = layout_object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsLayoutInline() ||
        ToLayoutBoxModelObject(child)->HasSelfPaintingLayer())
      continue;
    ObjectPainter(*child).AddURLRectIfNeeded(paint_info, paint_offset);
    AddURLRectsForInlineChildrenRecursively(*child, paint_info, paint_offset);
  }
}

bool LineBoxListPainter::ShouldPaint(const LayoutBoxModelObject& layout_object,
                                     const PaintInfo& paint_info,
                                     const PhysicalOffset& paint_offset) const {
  DCHECK(!ShouldPaintSelfOutline(paint_info.phase) &&
         !ShouldPaintDescendantOutlines(paint_info.phase));

  // The only way an inline could paint like this is if it has a layer.
  DCHECK(layout_object.IsLayoutBlock() ||
         (layout_object.IsLayoutInline() && layout_object.HasLayer()));

  if (paint_info.phase == PaintPhase::kForeground &&
      paint_info.ShouldAddUrlMetadata()) {
    AddURLRectsForInlineChildrenRecursively(layout_object, paint_info,
                                            paint_offset);
  }

  // If we have no lines then we have no work to do.
  if (!line_box_list_.First())
    return false;

  if (!line_box_list_.AnyLineIntersectsRect(
          LineLayoutBoxModel(const_cast<LayoutBoxModelObject*>(&layout_object)),
          paint_info.GetCullRect(), paint_offset))
    return false;

  return true;
}

void LineBoxListPainter::Paint(const LayoutBoxModelObject& layout_object,
                               const PaintInfo& paint_info,
                               const PhysicalOffset& paint_offset) const {
  // Only paint during the foreground/selection phases.
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kTextClip &&
      paint_info.phase != PaintPhase::kMask)
    return;

  if (!ShouldPaint(layout_object, paint_info, paint_offset))
    return;

  ScopedPaintTimingDetectorBlockPaintHook
      scoped_paint_timing_detector_block_paint_hook;
  if (paint_info.phase == PaintPhase::kForeground) {
    scoped_paint_timing_detector_block_paint_hook.EmplaceIfNeeded(
        layout_object,
        paint_info.context.GetPaintController().CurrentPaintChunkProperties());
  }

  // See if our root lines intersect with the dirty rect. If so, then we paint
  // them. Note that boxes can easily overlap, so we can't make any assumptions
  // based off positions of our first line box or our last line box.
  for (InlineFlowBox* curr : line_box_list_) {
    if (line_box_list_.LineIntersectsDirtyRect(
            LineLayoutBoxModel(
                const_cast<LayoutBoxModelObject*>(&layout_object)),
            curr, paint_info.GetCullRect(), paint_offset)) {
      RootInlineBox& root = curr->Root();
      curr->Paint(paint_info, paint_offset.ToLayoutPoint(), root.LineTop(),
                  root.LineBottom());
    }
  }
}

void LineBoxListPainter::PaintBackplate(
    const LayoutBoxModelObject& layout_object,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) const {
  if (paint_info.phase != PaintPhase::kForcedColorsModeBackplate ||
      !ShouldPaint(layout_object, paint_info, paint_offset))
    return;

  // Only paint backplates behind text when forced-color-adjust is auto.
  const ComputedStyle& style =
      line_box_list_.First()->GetLineLayoutItem().StyleRef();
  if (style.ForcedColorAdjust() == EForcedColorAdjust::kNone)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_object,
          DisplayItem::kForcedColorsModeBackplate))
    return;

  DrawingRecorder recorder(paint_info.context, layout_object,
                           DisplayItem::kForcedColorsModeBackplate);
  Color backplate_color = style.ForcedBackplateColor();
  const auto& backplates = GetBackplates(paint_offset);
  for (const auto backplate : backplates)
    paint_info.context.FillRect(FloatRect(backplate), backplate_color);
}

Vector<PhysicalRect> LineBoxListPainter::GetBackplates(
    const PhysicalOffset& paint_offset) const {
  Vector<PhysicalRect> paragraph_backplates;
  PhysicalRect current_backplate;
  int consecutive_line_breaks = 0;
  for (const InlineFlowBox* line : line_box_list_) {
    // Recursively build up and paint backplates for line boxes containing text.
    BuildBackplate(line, paint_offset, &current_backplate,
                   &consecutive_line_breaks, &paragraph_backplates);
  }
  if (!current_backplate.IsEmpty())
    paragraph_backplates.push_back(current_backplate);
  return paragraph_backplates;
}

}  // namespace blink
