/*
 * Copyright (C) 2012 Victor Carbune (victor@rosedu.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_vtt_cue.h"

#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/media_controls.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

class SnapToLinesLayouter {
  STACK_ALLOCATED();

 public:
  SnapToLinesLayouter(LayoutVTTCue& cue_box, const IntRect& controls_rect)
      : cue_box_(cue_box), controls_rect_(controls_rect), margin_(0.0) {
    if (Settings* settings = cue_box_.GetDocument().GetSettings())
      margin_ = settings->GetTextTrackMarginPercentage() / 100.0;
  }

  void UpdateLayout();

 private:
  bool IsOutside(const IntRect&) const;
  bool IsOverlapping() const;
  LayoutUnit ComputeInitialPositionAdjustment(LayoutUnit&,
                                              LayoutUnit,
                                              LayoutUnit) const;
  bool ShouldSwitchDirection(InlineFlowBox*, LayoutUnit, LayoutUnit) const;

  void MoveBoxesBy(LayoutUnit distance) {
    cue_box_.SetLogicalTop(cue_box_.LogicalTop() + distance);
  }

  InlineFlowBox* FindFirstLineBox() const;

  LayoutPoint specified_position_;
  LayoutVTTCue& cue_box_;
  IntRect controls_rect_;
  double margin_;
};

InlineFlowBox* SnapToLinesLayouter::FindFirstLineBox() const {
  if (!cue_box_.FirstChild()->IsLayoutInline())
    return nullptr;
  return ToLayoutInline(cue_box_.FirstChild())->FirstLineBox();
}

LayoutUnit SnapToLinesLayouter::ComputeInitialPositionAdjustment(
    LayoutUnit& step,
    LayoutUnit max_dimension,
    LayoutUnit margin) const {
  DCHECK(std::isfinite(cue_box_.SnapToLinesPosition()));

  // 6. Let line be cue's computed line.
  // 7. Round line to an integer by adding 0.5 and then flooring it.
  LayoutUnit line_position(floorf(cue_box_.SnapToLinesPosition() + 0.5f));

  WritingMode writing_mode = cue_box_.StyleRef().GetWritingMode();
  // 8. Vertical Growing Left: Add one to line then negate it.
  if (IsFlippedBlocksWritingMode(writing_mode))
    line_position = -(line_position + 1);

  // 9. Let position be the result of multiplying step and line offset.
  LayoutUnit position = step * line_position;

  // 10. Vertical Growing Left: Decrease position by the width of the
  // bounding box of the boxes in boxes, then increase position by step.
  if (IsFlippedBlocksWritingMode(writing_mode)) {
    position -= cue_box_.Size().Width();
    position += step;
  }

  // 11. If line is less than zero...
  if (line_position < 0) {
    // ... then increase position by max dimension ...
    position += max_dimension;

    // ... and negate step.
    step = -step;
  } else {
    // ... Otherwise, increase position by margin.
    position += margin;
  }
  return position;
}

// We use this helper to make sure all (bounding) boxes used for comparisons
// are relative to the same coordinate space. If we didn't the (bounding) boxes
// could be affect by transforms on an ancestor et.c, which could yield
// incorrect results.
IntRect BorderBoxRelativeToAncestor(const LayoutBox& box,
                                    const LayoutBoxModelObject& ancestor) {
  PhysicalRect border_box = box.PhysicalBorderBoxRect();
  // We pass UseTransforms here primarily because we use a transform for
  // non-snap-to-lines positioning (see VTTCue.cpp.)
  return EnclosingIntRect(box.LocalToAncestorRect(border_box, &ancestor));
}

IntRect CueBoundingBox(const LayoutBox& cue_box) {
  return BorderBoxRelativeToAncestor(cue_box, *cue_box.ContainingBlock());
}

bool SnapToLinesLayouter::IsOutside(const IntRect& title_area) const {
  return !title_area.Contains(CueBoundingBox(cue_box_));
}

bool SnapToLinesLayouter::IsOverlapping() const {
  IntRect cue_box_rect = CueBoundingBox(cue_box_);
  for (LayoutBox* box = cue_box_.PreviousSiblingBox(); box;
       box = box->PreviousSiblingBox()) {
    if (cue_box_rect.Intersects(CueBoundingBox(*box)))
      return true;
  }
  return cue_box_rect.Intersects(controls_rect_);
}

bool SnapToLinesLayouter::ShouldSwitchDirection(InlineFlowBox* first_line_box,
                                                LayoutUnit step,
                                                LayoutUnit margin) const {
  // 17. Horizontal: If step is negative and the top of the first line box in
  // boxes is now above the top of the title area, or if step is positive and
  // the bottom of the first line box in boxes is now below the bottom of the
  // title area, jump to the step labeled switch direction.
  // Vertical: If step is negative and the left edge of the first line
  // box in boxes is now to the left of the left edge of the title area, or
  // if step is positive and the right edge of the first line box in boxes is
  // now to the right of the right edge of the title area, jump to the step
  // labeled switch direction.
  LayoutUnit logical_top = cue_box_.LogicalTop();
  if (step < 0 && logical_top < margin)
    return true;
  if (step > 0 && logical_top + first_line_box->LogicalHeight() + margin >
                      cue_box_.ContainingBlock()->LogicalHeight())
    return true;
  return false;
}

void SnapToLinesLayouter::UpdateLayout() {
  // http://dev.w3.org/html5/webvtt/#dfn-apply-webvtt-cue-settings
  // Step 13, "If cue's text track cue snap-to-lines flag is set".

  InlineFlowBox* first_line_box = FindFirstLineBox();
  if (!first_line_box)
    return;

  // 1. Horizontal: Let margin be a user-agent-defined vertical length which
  // will be used to define a margin at the top and bottom edges of the video
  // into which cues will not be placed.
  //    Vertical: Let margin be a user-agent-defined horizontal length which
  // will be used to define a margin at the top and bottom edges of the video
  // into which cues will not be placed.
  // 2. Horizontal: Let full dimension be the height of video's rendering area
  //    Vertical: Let full dimension be the width of video's rendering area.
  WritingMode writing_mode = cue_box_.StyleRef().GetWritingMode();
  LayoutBlock* parent_block = cue_box_.ContainingBlock();
  LayoutUnit full_dimension = blink::IsHorizontalWritingMode(writing_mode)
                                  ? parent_block->Size().Height()
                                  : parent_block->Size().Width();
  LayoutUnit margin(full_dimension * margin_);

  // 3. Let max dimension be full dimension - (2 * margin)
  LayoutUnit max_dimension = full_dimension - 2 * margin;

  // 4. Horizontal: Let step be the height of the first line box in boxes.
  //    Vertical: Let step be the width of the first line box in boxes.
  LayoutUnit step = first_line_box->LogicalHeight();

  // 5. If step is zero, then jump to the step labeled done positioning below.
  if (!step)
    return;

  // Steps 6-11.
  LayoutUnit position_adjustment =
      ComputeInitialPositionAdjustment(step, max_dimension, margin);

  // 12. Move all boxes in boxes ...
  // Horizontal: ... down by the distance given by position
  // Vertical: ... right by the distance given by position
  MoveBoxesBy(position_adjustment);

  // 13. Remember the position of all the boxes in boxes as their specified
  // position.
  specified_position_ = cue_box_.Location();

  // XX. Let switched be false.
  bool switched = false;

  // 14. Horizontal: Let title area be a box that covers all of the video's
  // rendering area except for a height of margin at the top of the rendering
  // area and a height of margin at the bottom of the rendering area.
  // Vertical: Let title area be a box that covers all of the video’s
  // rendering area except for a width of margin at the left of the rendering
  // area and a width of margin at the right of the rendering area.
  IntRect title_area =
      EnclosingIntRect(cue_box_.ContainingBlock()->PhysicalBorderBoxRect());
  if (blink::IsHorizontalWritingMode(writing_mode)) {
    title_area.Move(0, margin.ToInt());
    title_area.Contract(0, (2 * margin).ToInt());
  } else {
    title_area.Move(margin.ToInt(), 0);
    title_area.Contract((2 * margin).ToInt(), 0);
  }

  // 15. Step loop: If none of the boxes in boxes would overlap any of the
  // boxes in output, and all of the boxes in output are entirely within the
  // title area box, then jump to the step labeled done positioning below.
  while (IsOutside(title_area) || IsOverlapping()) {
    // 16. Let current position score be the percentage of the area of the
    // bounding box of the boxes in boxes that is outside the title area
    // box.
    if (!ShouldSwitchDirection(first_line_box, step, margin)) {
      // 18. Horizontal: Move all the boxes in boxes down by the distance
      // given by step. (If step is negative, then this will actually
      // result in an upwards movement of the boxes in absolute terms.)
      // Vertical: Move all the boxes in boxes right by the distance
      // given by step. (If step is negative, then this will actually
      // result in a leftwards movement of the boxes in absolute terms.)
      MoveBoxesBy(step);

      // 19. Jump back to the step labeled step loop.
      continue;
    }

    // 20. Switch direction: If switched is true, then remove all the boxes
    // in boxes, and jump to the step labeled done positioning below.
    if (switched) {
      // This does not "remove" the boxes, but rather just pushes them
      // out of the viewport. Otherwise we'd need to mutate the layout
      // tree during layout.
      cue_box_.SetLogicalTop(cue_box_.ContainingBlock()->LogicalHeight() + 1);
      break;
    }

    // 21. Otherwise, move all the boxes in boxes back to their specified
    // position as determined in the earlier step.
    cue_box_.SetLocation(specified_position_);

    // 22. Negate step.
    step = -step;

    // 23. Set switched to true.
    switched = true;

    // 24. Jump back to the step labeled step loop.
  }
}

}  // unnamed namespace

LayoutVTTCue::LayoutVTTCue(ContainerNode* node, float snap_to_lines_position)
    : LayoutBlockFlow(node), snap_to_lines_position_(snap_to_lines_position) {}

void LayoutVTTCue::RepositionCueSnapToLinesNotSet() {
  // FIXME: Implement overlapping detection when snap-to-lines is not set.
  // http://wkb.ug/84296

  // http://dev.w3.org/html5/webvtt/#dfn-apply-webvtt-cue-settings
  // Step 13, "If cue's text track cue snap-to-lines flag is not set".

  // 1. Let bounding box be the bounding box of the boxes in boxes.

  // 2. Run the appropriate steps from the following list:
  //    If the text track cue writing direction is horizontal
  //       If the text track cue line alignment is middle alignment
  //          Move all the boxes in boxes up by half of the height of
  //          bounding box.
  //       If the text track cue line alignment is end alignment
  //          Move all the boxes in boxes up by the height of bounding box.
  //
  //    If the text track cue writing direction is vertical growing left or
  //    vertical growing right
  //       If the text track cue line alignment is middle alignment
  //          Move all the boxes in boxes left by half of the width of
  //          bounding box.
  //       If the text track cue line alignment is end alignment
  //          Move all the boxes in boxes left by the width of bounding box.

  // 3. If none of the boxes in boxes would overlap any of the boxes in
  // output, and all the boxes in output are within the video's rendering
  // area, then jump to the step labeled done positioning below.

  // 4. If there is a position to which the boxes in boxes can be moved while
  // maintaining the relative positions of the boxes in boxes to each other
  // such that none of the boxes in boxes would overlap any of the boxes in
  // output, and all the boxes in output would be within the video's
  // rendering area, then move the boxes in boxes to the closest such
  // position to their current position, and then jump to the step labeled
  // done positioning below. If there are multiple such positions that are
  // equidistant from their current position, use the highest one amongst
  // them; if there are several at that height, then use the leftmost one
  // amongst them.

  // 5. Otherwise, jump to the step labeled done positioning below. (The
  // boxes will unfortunately overlap.)
}

IntRect LayoutVTTCue::ComputeControlsRect() const {
  // Determine the area covered by the media controls, if any. For this, the
  // LayoutVTTCue will walk the tree up to the HTMLMediaElement, then ask for
  // the MediaControls.
  DCHECK(Parent()->GetNode()->IsTextTrackContainer());

  auto* media_element = To<HTMLMediaElement>(Parent()->Parent()->GetNode());
  DCHECK(media_element);

  MediaControls* controls = media_element->GetMediaControls();
  if (!controls || !controls->ContainerLayoutObject())
    return IntRect();

  // Only a part of the media controls is used for overlap avoidance.
  LayoutObject* button_panel_layout_object =
      media_element->GetMediaControls()->ButtonPanelLayoutObject();
  LayoutObject* timeline_layout_object =
      media_element->GetMediaControls()->TimelineLayoutObject();

  if (!button_panel_layout_object || !button_panel_layout_object->IsBox() ||
      !timeline_layout_object || !timeline_layout_object->IsBox()) {
    return IntRect();
  }

  IntRect button_panel_box = BorderBoxRelativeToAncestor(
      ToLayoutBox(*button_panel_layout_object),
      ToLayoutBox(*controls->ContainerLayoutObject()));
  IntRect timeline_box = BorderBoxRelativeToAncestor(
      ToLayoutBox(*timeline_layout_object),
      ToLayoutBox(*controls->ContainerLayoutObject()));

  button_panel_box.Unite(timeline_box);
  return button_panel_box;
}

void LayoutVTTCue::UpdateLayout() {
  LayoutBlockFlow::UpdateLayout();

  DCHECK(FirstChild());

  LayoutState state(*this);

  // http://dev.w3.org/html5/webvtt/#dfn-apply-webvtt-cue-settings - step 13.
  if (!std::isnan(snap_to_lines_position_))
    SnapToLinesLayouter(*this, ComputeControlsRect()).UpdateLayout();
  else
    RepositionCueSnapToLinesNotSet();
}

}  // namespace blink
