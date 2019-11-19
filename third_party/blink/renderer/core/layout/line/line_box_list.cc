/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/line/line_box_list.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_box.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_box_model.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_inline.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/paint/inline_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"

namespace blink {

#if DCHECK_IS_ON()
template <typename InlineBoxType>
void InlineBoxList<InlineBoxType>::AssertIsEmpty() {
  DCHECK(!first_);
  DCHECK(!last_);
}
#endif

const LineBoxList& LineBoxList::Empty() {
  // Need to use "static" because DISALLOW_NEW.
  static const LineBoxList empty;
  return empty;
}

const InlineTextBoxList& InlineTextBoxList::Empty() {
  // Need to use "static" because DISALLOW_NEW.
  static const InlineTextBoxList empty;
  return empty;
}

template <typename InlineBoxType>
void InlineBoxList<InlineBoxType>::AppendLineBox(InlineBoxType* box) {
  if (!first_) {
    first_ = last_ = box;
  } else {
    last_->SetNextForSameLayoutObject(box);
    box->SetPreviousForSameLayoutObject(last_);
    last_ = box;
  }
}

void LineBoxList::DeleteLineBoxTree() {
  InlineFlowBox* line = first_;
  InlineFlowBox* next_line;
  while (line) {
    next_line = line->NextForSameLayoutObject();
    line->DeleteLine();
    line = next_line;
  }
  first_ = last_ = nullptr;
}

template <typename InlineBoxType>
void InlineBoxList<InlineBoxType>::ExtractLineBox(InlineBoxType* box) {
  last_ = box->PrevForSameLayoutObject();
  if (box == first_)
    first_ = nullptr;
  if (box->PrevForSameLayoutObject())
    box->PrevForSameLayoutObject()->SetNextForSameLayoutObject(nullptr);
  box->SetPreviousForSameLayoutObject(nullptr);
  for (InlineBoxType* curr = box; curr; curr = curr->NextForSameLayoutObject())
    curr->SetExtracted();
}

template <typename InlineBoxType>
void InlineBoxList<InlineBoxType>::AttachLineBox(InlineBoxType* box) {
  if (last_) {
    last_->SetNextForSameLayoutObject(box);
    box->SetPreviousForSameLayoutObject(last_);
  } else {
    first_ = box;
  }
  InlineBoxType* last = box;
  for (InlineBoxType* curr = box; curr;
       curr = curr->NextForSameLayoutObject()) {
    curr->SetExtracted(false);
    last = curr;
  }
  last_ = last;
}

template <typename InlineBoxType>
void InlineBoxList<InlineBoxType>::RemoveLineBox(InlineBoxType* box) {
  if (box == first_)
    first_ = box->NextForSameLayoutObject();
  if (box == last_)
    last_ = box->PrevForSameLayoutObject();
  if (InlineBoxType* next = box->NextForSameLayoutObject())
    next->SetPreviousForSameLayoutObject(box->PrevForSameLayoutObject());
  if (InlineBoxType* prev = box->PrevForSameLayoutObject())
    prev->SetNextForSameLayoutObject(box->NextForSameLayoutObject());
}

template <typename InlineBoxType>
void InlineBoxList<InlineBoxType>::DeleteLineBoxes() {
  if (first_) {
    InlineBoxType* next;
    for (InlineBoxType* curr = first_; curr; curr = next) {
      next = curr->NextForSameLayoutObject();
      curr->Destroy();
    }
    first_ = nullptr;
    last_ = nullptr;
  }
}

void LineBoxList::DirtyLineBoxes() {
  for (InlineFlowBox* curr : *this)
    curr->DirtyLineBoxes();
}

bool LineBoxList::RangeIntersectsRect(LineLayoutBoxModel layout_object,
                                      LayoutUnit logical_top,
                                      LayoutUnit logical_bottom,
                                      const CullRect& cull_rect,
                                      const PhysicalOffset& offset) const {
  LineLayoutBox block;
  if (layout_object.IsBox())
    block = LineLayoutBox(layout_object);
  else
    block = layout_object.ContainingBlock();
  LayoutUnit physical_start = block.FlipForWritingMode(logical_top);
  LayoutUnit physical_end = block.FlipForWritingMode(logical_bottom);
  LayoutUnit physical_extent = AbsoluteValue(physical_end - physical_start);
  physical_start = std::min(physical_start, physical_end);

  if (layout_object.StyleRef().IsHorizontalWritingMode()) {
    physical_start += offset.top;
    return cull_rect.IntersectsVerticalRange(physical_start,
                                             physical_start + physical_extent);
  } else {
    physical_start += offset.left;
    return cull_rect.IntersectsHorizontalRange(
        physical_start, physical_start + physical_extent);
  }
}

bool LineBoxList::AnyLineIntersectsRect(LineLayoutBoxModel layout_object,
                                        const CullRect& cull_rect,
                                        const PhysicalOffset& offset) const {
  // We can check the first box and last box and avoid painting/hit testing if
  // we don't intersect. This is a quick short-circuit that we can take to avoid
  // walking any lines.
  // FIXME: This check is flawed in the following extremely obscure way:
  // if some line in the middle has a huge overflow, it might actually extend
  // below the last line.
  RootInlineBox& first_root_box = First()->Root();
  RootInlineBox& last_root_box = Last()->Root();
  LayoutUnit first_line_top =
      First()->LogicalTopVisualOverflow(first_root_box.LineTop());
  LayoutUnit last_line_bottom =
      Last()->LogicalBottomVisualOverflow(last_root_box.LineBottom());

  return RangeIntersectsRect(layout_object, first_line_top, last_line_bottom,
                             cull_rect, offset);
}

bool LineBoxList::LineIntersectsDirtyRect(LineLayoutBoxModel layout_object,
                                          InlineFlowBox* box,
                                          const CullRect& cull_rect,
                                          const PhysicalOffset& offset) const {
  RootInlineBox& root = box->Root();
  LayoutUnit logical_top = std::min<LayoutUnit>(
      box->LogicalTopVisualOverflow(root.LineTop()), root.SelectionTop());
  LayoutUnit logical_bottom =
      box->LogicalBottomVisualOverflow(root.LineBottom());

  return RangeIntersectsRect(layout_object, logical_top, logical_bottom,
                             cull_rect, offset);
}

bool LineBoxList::HitTest(LineLayoutBoxModel layout_object,
                          HitTestResult& result,
                          const HitTestLocation& hit_test_location,
                          const PhysicalOffset& accumulated_offset,
                          HitTestAction hit_test_action) const {
  if (hit_test_action != kHitTestForeground)
    return false;

  // The only way an inline could hit test like this is if it has a layer.
  DCHECK(layout_object.IsLayoutBlock() ||
         (layout_object.IsLayoutInline() && layout_object.HasLayer()));

  // If we have no lines then we have no work to do.
  if (!First())
    return false;

  const PhysicalOffset& point = hit_test_location.Point();
  IntRect hit_search_bounding_box = hit_test_location.EnclosingIntRect();

  CullRect cull_rect(
      First()->IsHorizontal()
          ? IntRect(point.left.ToInt(), hit_search_bounding_box.Y(), 1,
                    hit_search_bounding_box.Height())
          : IntRect(hit_search_bounding_box.X(), point.top.ToInt(),
                    hit_search_bounding_box.Width(), 1));

  if (!AnyLineIntersectsRect(layout_object, cull_rect, accumulated_offset))
    return false;

  // See if our root lines contain the point. If so, then we hit test them
  // further. Note that boxes can easily overlap, so we can't make any
  // assumptions based off positions of our first line box or our last line box.
  for (InlineFlowBox* curr : InReverseOrder()) {
    RootInlineBox& root = curr->Root();
    if (RangeIntersectsRect(
            layout_object, curr->LogicalTopVisualOverflow(root.LineTop()),
            curr->LogicalBottomVisualOverflow(root.LineBottom()), cull_rect,
            accumulated_offset)) {
      bool inside =
          curr->NodeAtPoint(result, hit_test_location, accumulated_offset,
                            root.LineTop(), root.LineBottom());
      if (inside) {
        layout_object.UpdateHitTestResult(
            result, hit_test_location.Point() - accumulated_offset);
        return true;
      }
    }
  }

  return false;
}

void LineBoxList::DirtyLinesFromChangedChild(LineLayoutItem container,
                                             LineLayoutItem child,
                                             bool can_dirty_ancestors) {
  if (!container.Parent() ||
      (container.IsLayoutBlock() &&
       (container.SelfNeedsLayout() || !container.IsLayoutBlockFlow())))
    return;

  LineLayoutInline inline_container = container.IsLayoutInline()
                                          ? LineLayoutInline(container)
                                          : LineLayoutInline();

  // If we are attaching children dirtying lines is unnecessary as we will do a
  // full layout of the inline's contents anyway.
  if (inline_container && !inline_container.EverHadLayout())
    return;

  InlineBox* first_box = inline_container
                             ? inline_container.FirstLineBoxIncludingCulling()
                             : First();

  // If we have no first line box, then just bail early.
  if (!first_box) {
    // For an empty inline, go ahead and propagate the check up to our parent,
    // unless the parent is already dirty.
    if (container.IsInline() && !container.AncestorLineBoxDirty() &&
        can_dirty_ancestors) {
      container.Parent().DirtyLinesFromChangedChild(container);
      // Mark the container to avoid dirtying the same lines again across
      // multiple destroy() calls of the same subtree.
      container.SetAncestorLineBoxDirty();
    }
    return;
  }

  // Try to figure out which line box we belong in. First try to find a previous
  // line box by examining our siblings. If we are a float inside an inline then
  // check our nearest inline ancestor with siblings. If we didn't find a line
  // box, then use our parent's first line box.
  RootInlineBox* box = nullptr;
  LineLayoutItem curr = child.PreviousSibling();
  if (child.IsFloating() && !curr) {
    DCHECK(child.Parent());
    if (child.Parent().IsLayoutInline()) {
      LineLayoutItem outer_inline = child.Parent();
      while (outer_inline && !outer_inline.PreviousSibling() &&
             outer_inline.Parent().IsLayoutInline())
        outer_inline = outer_inline.Parent();
      if (outer_inline)
        curr = outer_inline.PreviousSibling();
    }
  }

  for (; curr; curr = curr.PreviousSibling()) {
    if (curr.IsFloatingOrOutOfFlowPositioned())
      continue;

    if (curr.IsAtomicInlineLevel()) {
      InlineBox* wrapper = LineLayoutBox(curr).InlineBoxWrapper();
      if (wrapper)
        box = &wrapper->Root();
    } else if (curr.IsText()) {
      InlineTextBox* text_box = LineLayoutText(curr).LastTextBox();
      if (text_box)
        box = &text_box->Root();
    } else if (curr.IsLayoutInline()) {
      InlineBox* last_sibling_box =
          LineLayoutInline(curr).LastLineBoxIncludingCulling();
      if (last_sibling_box)
        box = &last_sibling_box->Root();
    }

    if (box)
      break;
  }
  if (!box) {
    if (inline_container && !inline_container.AlwaysCreateLineBoxes()) {
      // https://bugs.webkit.org/show_bug.cgi?id=60778
      // We may have just removed a <br> with no line box that was our first
      // child. In this case we won't find a previous sibling, but firstBox can
      // be pointing to a following sibling. This isn't good enough, since we
      // won't locate the root line box that encloses the removed <br>. We have
      // to just over-invalidate a bit and go up to our parent.
      if (!inline_container.AncestorLineBoxDirty() && can_dirty_ancestors) {
        inline_container.Parent().DirtyLinesFromChangedChild(inline_container);
        // Mark the container to avoid dirtying the same lines again across
        // multiple destroy() calls of the same subtree.
        inline_container.SetAncestorLineBoxDirty();
      }
      return;
    }
    box = &first_box->Root();
  }

  // If we found a line box, then dirty it.
  if (box) {
    box->MarkDirty();

    // dirty the adjacent lines that might be affected
    // NOTE: we dirty the previous line because RootInlineBox objects cache
    // the address of the first object on the next line after a BR, which we may
    // be invalidating here. For more info, see how LayoutBlock::
    // layoutInlineChildren calls setLineBreakInfo with the result of
    // findNextLineBreak. findNextLineBreak, despite the name, actually returns
    // the first LayoutObject after the BR. <rdar://problem/3849947> "Typing
    // after pasting line does not appear until after window resize."
    if (RootInlineBox* prev_root_box = box->PrevRootBox())
      prev_root_box->MarkDirty();
    // If |child| or any of its immediately previous siblings with culled
    // lineboxes is the object after a line-break in |box| or the linebox after
    // it then that means |child| actually sits on the linebox after |box| (or
    // is its line-break object) and so we need to dirty it as well.
    if (RootInlineBox* next_root_box = box->NextRootBox())
      next_root_box->MarkDirty();
  }
}

template class InlineBoxList<InlineFlowBox>;
template class InlineBoxList<InlineTextBox>;

}  // namespace blink
