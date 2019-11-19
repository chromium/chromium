/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/line/inline_box.h"

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/line/inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

class LayoutObject;

struct SameSizeAsInlineBox : DisplayItemClient {
  ~SameSizeAsInlineBox() override = default;
  void* a[4];
  LayoutPoint b;
  LayoutUnit c;
  uint32_t bitfields;
#if DCHECK_IS_ON()
  bool f;
#endif
};

static_assert(sizeof(InlineBox) == sizeof(SameSizeAsInlineBox),
              "InlineBox should stay small");

#if DCHECK_IS_ON()
InlineBox::~InlineBox() {
  if (!has_bad_parent_ && parent_)
    parent_->SetHasBadChildList();
}
#endif

DISABLE_CFI_PERF
void InlineBox::Destroy() {
  // We do not need to issue invalidations if the page is being destroyed
  // since these objects will never be repainted.
  if (!line_layout_item_.DocumentBeingDestroyed()) {
    SetLineLayoutItemShouldDoFullPaintInvalidationIfNeeded();

    // TODO(crbug.com/619630): Make this fast.
    line_layout_item_.SlowSetPaintingLayerNeedsRepaint();
  }

  delete this;
}

void InlineBox::Remove(MarkLineBoxes mark_line_boxes) {
  if (Parent())
    Parent()->RemoveChild(this, mark_line_boxes);
}

void* InlineBox::operator new(size_t sz) {
  return WTF::Partitions::LayoutPartition()->Alloc(
      sz, WTF_HEAP_PROFILER_TYPE_NAME(InlineBox));
}

void InlineBox::operator delete(void* ptr) {
  base::PartitionFree(ptr);
}

const char* InlineBox::BoxName() const {
  return "InlineBox";
}

String InlineBox::DebugName() const {
  return BoxName();
}

IntRect InlineBox::VisualRect() const {
  return GetLineLayoutItem().VisualRectForInlineBox();
}

IntRect InlineBox::PartialInvalidationVisualRect() const {
  return GetLineLayoutItem().PartialInvalidationVisualRectForInlineBox();
}

#if DCHECK_IS_ON()
void InlineBox::ShowTreeForThis() const {
  GetLineLayoutItem().ShowTreeForThis();
}

void InlineBox::ShowLineTreeForThis() const {
  const LayoutBlock* containing_block =
      LineLayoutAPIShim::LayoutObjectFrom(GetLineLayoutItem())
          ->InclusiveContainingBlock();
  if (containing_block) {
    LineLayoutBox(const_cast<LayoutBlock*>(containing_block))
        .ShowLineTreeAndMark(this, "*");
  }
}

void InlineBox::DumpLineTreeAndMark(StringBuilder& string_builder,
                                    const InlineBox* marked_box1,
                                    const char* marked_label1,
                                    const InlineBox* marked_box2,
                                    const char* marked_label2,
                                    const LayoutObject* obj,
                                    int depth) const {
  StringBuilder string_inlinebox;
  if (this == marked_box1)
    string_inlinebox.Append(marked_label1);
  if (this == marked_box2)
    string_inlinebox.Append(marked_label2);
  if (GetLineLayoutItem().IsEqual(obj))
    string_inlinebox.Append('*');
  while ((int)string_inlinebox.length() < (depth * 2))
    string_inlinebox.Append(' ');

  DumpBox(string_inlinebox);
  string_builder.Append('\n');
  string_builder.Append(string_inlinebox);
}

void InlineBox::DumpBox(StringBuilder& string_inlinebox) const {
  string_inlinebox.AppendFormat("%s %p", BoxName(), this);
  while (string_inlinebox.length() < kShowTreeCharacterOffset)
    string_inlinebox.Append(' ');
  string_inlinebox.AppendFormat(
      "\t%s %p {pos=%g,%g size=%g,%g} baseline=%i/%i",
      GetLineLayoutItem().DecoratedName().Ascii().c_str(),
      GetLineLayoutItem().DebugPointer(), X().ToFloat(), Y().ToFloat(),
      Width().ToFloat(), Height().ToFloat(),
      BaselinePosition(kAlphabeticBaseline).ToInt(),
      BaselinePosition(kIdeographicBaseline).ToInt());
}
#endif  // DCHECK_IS_ON()

LayoutUnit InlineBox::LogicalHeight() const {
  if (HasVirtualLogicalHeight())
    return VirtualLogicalHeight();

  const SimpleFontData* font_data =
      GetLineLayoutItem().Style(IsFirstLineStyle())->GetFont().PrimaryFont();
  if (GetLineLayoutItem().IsText()) {
    DCHECK(font_data);
    return bitfields_.IsText() && font_data
               ? LayoutUnit(font_data->GetFontMetrics().Height())
               : LayoutUnit();
  }
  if (GetLineLayoutItem().IsBox() && Parent()) {
    return IsHorizontal() ? LineLayoutBox(GetLineLayoutItem()).Size().Height()
                          : LineLayoutBox(GetLineLayoutItem()).Size().Width();
  }

  DCHECK(IsInlineFlowBox());
  LineLayoutBoxModel flow_object = BoxModelObject();
  DCHECK(font_data);
  LayoutUnit result(font_data ? font_data->GetFontMetrics().Height() : 0);
  if (Parent())
    result += flow_object.BorderAndPaddingLogicalHeight();
  return result;
}

LayoutUnit InlineBox::BaselinePosition(FontBaseline baseline_type) const {
  return BoxModelObject().BaselinePosition(
      baseline_type, bitfields_.FirstLine(),
      IsHorizontal() ? kHorizontalLine : kVerticalLine,
      kPositionOnContainingLine);
}

LayoutUnit InlineBox::LineHeight() const {
  return BoxModelObject().LineHeight(
      bitfields_.FirstLine(), IsHorizontal() ? kHorizontalLine : kVerticalLine,
      kPositionOnContainingLine);
}

int InlineBox::CaretMinOffset() const {
  return GetLineLayoutItem().CaretMinOffset();
}

int InlineBox::CaretMaxOffset() const {
  return GetLineLayoutItem().CaretMaxOffset();
}

void InlineBox::DirtyLineBoxes() {
  MarkDirty();
  for (InlineFlowBox* curr = Parent(); curr && !curr->IsDirty();
       curr = curr->Parent())
    curr->MarkDirty();
}

void InlineBox::DeleteLine() {
  if (!bitfields_.Extracted() && GetLineLayoutItem().IsBox())
    LineLayoutBox(GetLineLayoutItem()).SetInlineBoxWrapper(nullptr);
  Destroy();
}

void InlineBox::ExtractLine() {
  bitfields_.SetExtracted(true);
  if (GetLineLayoutItem().IsBox())
    LineLayoutBox(GetLineLayoutItem()).SetInlineBoxWrapper(nullptr);
}

void InlineBox::AttachLine() {
  bitfields_.SetExtracted(false);
  if (GetLineLayoutItem().IsBox())
    LineLayoutBox(GetLineLayoutItem()).SetInlineBoxWrapper(this);
}

void InlineBox::Move(const LayoutSize& delta) {
  location_.Move(delta);

  if (GetLineLayoutItem().IsAtomicInlineLevel())
    LineLayoutBox(GetLineLayoutItem()).Move(delta.Width(), delta.Height());

  SetLineLayoutItemShouldDoFullPaintInvalidationIfNeeded();
}

void InlineBox::Paint(const PaintInfo& paint_info,
                      const LayoutPoint&,
                      LayoutUnit,
                      LayoutUnit) const {
  BlockPainter::PaintInlineBox(*this, paint_info);
}

bool InlineBox::NodeAtPoint(HitTestResult& result,
                            const HitTestLocation& hit_test_location,
                            const PhysicalOffset& accumulated_offset,
                            LayoutUnit /* lineTop */,
                            LayoutUnit /* lineBottom */) {
  // Hit test all phases of replaced elements atomically, as though the replaced
  // element established its own stacking context. (See Appendix E.2, section
  // 6.4 on inline block/table elements in the CSS2.1 specification.)
  PhysicalOffset layout_item_accumulated_offset = accumulated_offset;
  if (GetLineLayoutItem().IsBox()) {
    layout_item_accumulated_offset +=
        LineLayoutBox(GetLineLayoutItem()).PhysicalLocation();
  }
  return GetLineLayoutItem().HitTestAllPhases(result, hit_test_location,
                                              layout_item_accumulated_offset);
}

const RootInlineBox& InlineBox::Root() const {
  if (parent_)
    return parent_->Root();
  DCHECK(IsRootInlineBox());
  return static_cast<const RootInlineBox&>(*this);
}

RootInlineBox& InlineBox::Root() {
  if (parent_)
    return parent_->Root();
  DCHECK(IsRootInlineBox());
  return static_cast<RootInlineBox&>(*this);
}

InlineBox* InlineBox::NextLeafChild() const {
  InlineBox* leaf = nullptr;
  for (InlineBox* box = NextOnLine(); box && !leaf; box = box->NextOnLine())
    leaf = box->IsLeaf() ? box : ToInlineFlowBox(box)->FirstLeafChild();
  if (!leaf && Parent())
    leaf = Parent()->NextLeafChild();
  return leaf;
}

InlineBox* InlineBox::PrevLeafChild() const {
  InlineBox* leaf = nullptr;
  for (InlineBox* box = PrevOnLine(); box && !leaf; box = box->PrevOnLine())
    leaf = box->IsLeaf() ? box : ToInlineFlowBox(box)->LastLeafChild();
  if (!leaf && Parent())
    leaf = Parent()->PrevLeafChild();
  return leaf;
}

InlineBox* InlineBox::NextLeafChildIgnoringLineBreak() const {
  InlineBox* leaf = NextLeafChild();
  return (leaf && leaf->IsLineBreak()) ? nullptr : leaf;
}

InlineBox* InlineBox::PrevLeafChildIgnoringLineBreak() const {
  InlineBox* leaf = PrevLeafChild();
  return (leaf && leaf->IsLineBreak()) ? nullptr : leaf;
}

bool InlineBox::IsSelected() const {
  return GetLineLayoutItem().IsSelected();
}

bool InlineBox::CanAccommodateEllipsis(bool ltr,
                                       LayoutUnit block_edge,
                                       LayoutUnit ellipsis_width) const {
  // Non-atomic inline-level elements can always accommodate an ellipsis.
  // Skip list markers and try the next box.
  if (!GetLineLayoutItem().IsAtomicInlineLevel() ||
      GetLineLayoutItem().IsListMarker())
    return true;

  LayoutRect box_rect(X(), LayoutUnit(), logical_width_, LayoutUnit(10));
  LayoutRect ellipsis_rect(ltr ? block_edge - ellipsis_width : block_edge,
                           LayoutUnit(), ellipsis_width, LayoutUnit(10));
  return !(box_rect.Intersects(ellipsis_rect));
}

LayoutUnit InlineBox::PlaceEllipsisBox(bool,
                                       LayoutUnit,
                                       LayoutUnit,
                                       LayoutUnit,
                                       LayoutUnit& truncated_width,
                                       InlineBox**,
                                       LayoutUnit) {
  // Use -1 to mean "we didn't set the position."
  truncated_width += LogicalWidth();
  return LayoutUnit(-1);
}

void InlineBox::ClearKnownToHaveNoOverflow() {
  bitfields_.SetKnownToHaveNoOverflow(false);
  if (Parent() && Parent()->KnownToHaveNoOverflow())
    Parent()->ClearKnownToHaveNoOverflow();
}

PhysicalOffset InlineBox::PhysicalLocation() const {
  LayoutRect rect(Location(), Size());
  FlipForWritingMode(rect);
  return PhysicalOffset(rect.Location());
}

void InlineBox::FlipForWritingMode(LayoutRect& rect) const {
  if (!UNLIKELY(GetLineLayoutItem().HasFlippedBlocksWritingMode()))
    return;
  Root().Block().FlipForWritingMode(rect);
}

LayoutPoint InlineBox::FlipForWritingMode(const LayoutPoint& point) const {
  if (!UNLIKELY(GetLineLayoutItem().HasFlippedBlocksWritingMode()))
    return point;
  return Root().Block().FlipForWritingMode(point);
}

void InlineBox::SetShouldDoFullPaintInvalidationForFirstLine() {
  GetLineLayoutItem().StyleRef().ClearCachedPseudoElementStyles();
  GetLineLayoutItem().SetShouldDoFullPaintInvalidation();
  if (!IsInlineFlowBox())
    return;
  for (InlineBox* child = ToInlineFlowBox(this)->FirstChild(); child;
       child = child->NextOnLine())
    child->SetShouldDoFullPaintInvalidationForFirstLine();
}

void InlineBox::SetLineLayoutItemShouldDoFullPaintInvalidationIfNeeded() {
  // For RootInlineBox, we only need to invalidate if it's using the first line
  // style. Otherwise it paints nothing so we don't need to invalidate it.
  if (!IsRootInlineBox() || IsFirstLineStyle())
    line_layout_item_.SetShouldDoFullPaintInvalidation();
}

bool CanUseInlineBox(const LayoutObject& node) {
  DCHECK(node.IsText() || node.IsInline() || node.IsLayoutBlockFlow());
  return !RuntimeEnabledFeatures::LayoutNGEnabled() ||
         !node.ContainingNGBlockFlow();
}

}  // namespace blink

#if DCHECK_IS_ON()

void showTree(const blink::InlineBox* b) {
  if (b)
    b->ShowTreeForThis();
  else
    fprintf(stderr, "Cannot showTree for (nil) InlineBox.\n");
}

void showLineTree(const blink::InlineBox* b) {
  if (b)
    b->ShowLineTreeForThis();
  else
    fprintf(stderr, "Cannot showLineTree for (nil) InlineBox.\n");
}

#endif
