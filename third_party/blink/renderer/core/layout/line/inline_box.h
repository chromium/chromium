/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc.
 *               All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_BOX_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_box_model.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/platform/fonts/font_vertical_position_type.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

class HitTestRequest;
class HitTestResult;
class InlineFlowBox;
class LayoutObject;
class RootInlineBox;

enum MarkLineBoxes { kMarkLineBoxesDirty, kDontMarkLineBoxes };

// InlineBox represents a rectangle that occurs on a line.  It corresponds to
// some LayoutObject (i.e., it represents a portion of that LayoutObject).
class CORE_EXPORT InlineBox : public DisplayItemClient {
 public:
  InlineBox(LineLayoutItem obj)
      : next_(nullptr),
        prev_(nullptr),
        parent_(nullptr),
        line_layout_item_(obj),
        logical_width_() {}

  InlineBox(LineLayoutItem item,
            LayoutPoint top_left,
            LayoutUnit logical_width,
            bool first_line,
            bool constructed,
            bool dirty,
            bool extracted,
            bool is_horizontal,
            InlineBox* next,
            InlineBox* prev,
            InlineFlowBox* parent)
      : next_(next),
        prev_(prev),
        parent_(parent),
        line_layout_item_(item),
        location_(top_left),
        logical_width_(logical_width),
        bitfields_(first_line, constructed, dirty, extracted, is_horizontal) {}

  ~InlineBox() override;

  virtual void Destroy();

  virtual void DeleteLine();
  virtual void ExtractLine();
  virtual void AttachLine();

  virtual bool IsLineBreak() const { return false; }

  // These methods are called when the caller wants to move the position of
  // InlineBox without full layout of it. The implementation should update the
  // position of the whole subtree (e.g. position of descendants and overflow
  // etc. should also be moved accordingly).
  virtual void Move(const LayoutSize& delta);
  DISABLE_CFI_PERF void MoveInLogicalDirection(
      const LayoutSize& delta_in_logical_direction) {
    Move(IsHorizontal() ? delta_in_logical_direction
                        : delta_in_logical_direction.TransposedSize());
  }
  void MoveInInlineDirection(LayoutUnit delta) {
    MoveInLogicalDirection(LayoutSize(delta, LayoutUnit()));
  }
  void MoveInBlockDirection(LayoutUnit delta) {
    MoveInLogicalDirection(LayoutSize(LayoutUnit(), delta));
  }

  virtual void Paint(const PaintInfo&,
                     const LayoutPoint&,
                     LayoutUnit line_top,
                     LayoutUnit line_bottom) const;
  virtual bool NodeAtPoint(HitTestResult&,
                           const HitTestLocation&,
                           const PhysicalOffset& accumulated_offset,
                           LayoutUnit line_top,
                           LayoutUnit line_bottom);

  // InlineBoxes are allocated out of the rendering partition.
  void* operator new(size_t);
  void operator delete(void*);

#if DCHECK_IS_ON()
  void ShowTreeForThis() const;
  void ShowLineTreeForThis() const;

  virtual void DumpBox(StringBuilder&) const;
  virtual void DumpLineTreeAndMark(StringBuilder&,
                                   const InlineBox* = nullptr,
                                   const char* = nullptr,
                                   const InlineBox* = nullptr,
                                   const char* = nullptr,
                                   const LayoutObject* = nullptr,
                                   int = 0) const;
#endif

  virtual const char* BoxName() const;

  // DisplayItemClient methods
  String DebugName() const override;
  IntRect VisualRect() const override;
  IntRect PartialInvalidationVisualRect() const override;

  bool IsText() const { return bitfields_.IsText(); }
  void SetIsText(bool is_text) { bitfields_.SetIsText(is_text); }

  virtual bool IsInlineFlowBox() const { return false; }
  virtual bool IsInlineTextBox() const { return false; }
  virtual bool IsRootInlineBox() const { return false; }

  virtual bool IsSVGInlineTextBox() const { return false; }
  virtual bool IsSVGInlineFlowBox() const { return false; }
  virtual bool IsSVGRootInlineBox() const { return false; }

  bool HasVirtualLogicalHeight() const {
    return bitfields_.HasVirtualLogicalHeight();
  }
  void SetHasVirtualLogicalHeight() {
    bitfields_.SetHasVirtualLogicalHeight(true);
  }
  virtual LayoutUnit VirtualLogicalHeight() const {
    NOTREACHED();
    return LayoutUnit();
  }

  bool IsHorizontal() const { return bitfields_.IsHorizontal(); }
  void SetIsHorizontal(bool is_horizontal) {
    bitfields_.SetIsHorizontal(is_horizontal);
  }

  bool IsConstructed() { return bitfields_.Constructed(); }
  virtual void SetConstructed() { bitfields_.SetConstructed(true); }

  void SetExtracted(bool extracted = true) {
    bitfields_.SetExtracted(extracted);
  }

  void SetFirstLineStyleBit(bool first_line) {
    bitfields_.SetFirstLine(first_line);
  }
  bool IsFirstLineStyle() const { return bitfields_.FirstLine(); }
  const ComputedStyle& LineStyleRef() const {
    return GetLineLayoutItem().StyleRef(IsFirstLineStyle());
  }

  void Remove(MarkLineBoxes = kMarkLineBoxesDirty);

  InlineBox* NextOnLine() const { return next_; }
  InlineBox* PrevOnLine() const { return prev_; }
  void SetNextOnLine(InlineBox* next) {
    DCHECK(parent_ || !next);
    next_ = next;
  }
  void SetPrevOnLine(InlineBox* prev) {
    DCHECK(parent_ || !prev);
    prev_ = prev;
  }

  virtual bool IsLeaf() const { return true; }

  InlineBox* NextLeafChild() const;
  InlineBox* PrevLeafChild() const;

  // Helper functions for editing and hit-testing code.
  InlineBox* NextLeafChildIgnoringLineBreak() const;
  InlineBox* PrevLeafChildIgnoringLineBreak() const;

  LineLayoutItem GetLineLayoutItem() const { return line_layout_item_; }

  InlineFlowBox* Parent() const {
#if DCHECK_IS_ON()
    DCHECK(!has_bad_parent_);
#endif
    return parent_;
  }

  void SetParent(InlineFlowBox* par) { parent_ = par; }

  const RootInlineBox& Root() const;
  RootInlineBox& Root();

  // x() is the location of the box in the containing block's "physical
  // coordinates with flipped block-flow direction".
  // See ../README.md#Coordinate-Spaces for the definition.
  void SetX(LayoutUnit x) { location_.SetX(x); }
  LayoutUnit X() const { return location_.X(); }

  // y() is the top side of the box in the containing block's physical
  // coordinates. It's actually in the same coordinate space as x() but for y()
  // physical coordinates and "physical coordinates with flipped block-flow
  // direction" are the same.
  void SetY(LayoutUnit y) { location_.SetY(y); }
  LayoutUnit Y() const { return location_.Y(); }

  // x() and y() in one LayoutPoint, in the containing block's "physical
  // coordinates with flipped block-flow direction".
  const LayoutPoint& Location() const { return location_; }

  LayoutUnit Width() const {
    return IsHorizontal() ? LogicalWidth() : LogicalHeight();
  }
  LayoutUnit Height() const {
    return IsHorizontal() ? LogicalHeight() : LogicalWidth();
  }
  LayoutSize Size() const { return LayoutSize(Width(), Height()); }

  // The logicalLeft position is the left edge of the line box in a horizontal
  // line and the top edge in a vertical line.
  LayoutUnit LogicalLeft() const {
    return IsHorizontal() ? location_.X() : location_.Y();
  }
  LayoutUnit LogicalRight() const { return LogicalLeft() + LogicalWidth(); }
  void SetLogicalLeft(LayoutUnit left) {
    if (IsHorizontal())
      SetX(left);
    else
      SetY(left);
  }

  // The logicalTop[ position is the top edge of the line box in a horizontal
  // line and the left edge in a vertical line.
  LayoutUnit LogicalTop() const {
    return IsHorizontal() ? location_.Y() : location_.X();
  }
  LayoutUnit LogicalBottom() const { return LogicalTop() + LogicalHeight(); }
  void SetLogicalTop(LayoutUnit top) {
    if (IsHorizontal())
      SetY(top);
    else
      SetX(top);
  }

  // The logical width is our extent in the line's overall inline direction,
  // i.e., width for horizontal text and height for vertical text.
  void SetLogicalWidth(LayoutUnit w) { logical_width_ = w; }
  LayoutUnit LogicalWidth() const { return logical_width_; }

  // The logical height is our extent in the block flow direction, i.e., height
  // for horizontal text and width for vertical text.
  LayoutUnit LogicalHeight() const;

  LayoutRect LogicalFrameRect() const {
    return IsHorizontal() ? LayoutRect(location_.X(), location_.Y(),
                                       logical_width_, LogicalHeight())
                          : LayoutRect(location_.Y(), location_.X(),
                                       logical_width_, LogicalHeight());
  }

  virtual LayoutUnit BaselinePosition(FontBaseline baseline_type) const;
  virtual LayoutUnit LineHeight() const;

  virtual int CaretMinOffset() const;
  virtual int CaretMaxOffset() const;

  unsigned char BidiLevel() const { return bitfields_.BidiEmbeddingLevel(); }
  void SetBidiLevel(unsigned char level) {
    bitfields_.SetBidiEmbeddingLevel(level);
  }
  TextDirection Direction() const {
    return BidiLevel() % 2 ? TextDirection::kRtl : TextDirection::kLtr;
  }
  bool IsLeftToRightDirection() const {
    return Direction() == TextDirection::kLtr;
  }
  int CaretLeftmostOffset() const {
    return IsLeftToRightDirection() ? CaretMinOffset() : CaretMaxOffset();
  }
  int CaretRightmostOffset() const {
    return IsLeftToRightDirection() ? CaretMaxOffset() : CaretMinOffset();
  }

  virtual void ClearTruncation() {}

  bool IsDirty() const { return bitfields_.Dirty(); }
  virtual void MarkDirty() { bitfields_.SetDirty(true); }

  virtual void DirtyLineBoxes();

  virtual bool IsSelected() const;

  virtual bool CanAccommodateEllipsis(bool ltr,
                                      LayoutUnit block_edge,
                                      LayoutUnit ellipsis_width) const;
  // visibleLeftEdge, visibleRightEdge are in the parent's coordinate system.
  virtual LayoutUnit PlaceEllipsisBox(bool ltr,
                                      LayoutUnit visible_left_edge,
                                      LayoutUnit visible_right_edge,
                                      LayoutUnit ellipsis_width,
                                      LayoutUnit& truncated_width,
                                      InlineBox**,
                                      LayoutUnit logical_left_offset);

#if DCHECK_IS_ON()
  void SetHasBadParent();
#endif

  int Expansion() const { return bitfields_.Expansion(); }

  bool VisibleToHitTestRequest(const HitTestRequest& request) const {
    return GetLineLayoutItem().VisibleToHitTestRequest(request);
  }

  // Anonymous inline: https://drafts.csswg.org/css2/visuren.html#anonymous
  bool IsAnonymousInline() const {
    return GetLineLayoutItem().IsText() && GetLineLayoutItem().Parent() &&
           GetLineLayoutItem().Parent().IsBox();
  }
  EVerticalAlign VerticalAlign() const {
    return IsAnonymousInline()
               ? ComputedStyleInitialValues::InitialVerticalAlign()
               : GetLineLayoutItem()
                     .Style(bitfields_.FirstLine())
                     ->VerticalAlign();
  }

  // Use with caution! The type is not checked!
  LineLayoutBoxModel BoxModelObject() const {
    if (!GetLineLayoutItem().IsText())
      return LineLayoutBoxModel(line_layout_item_);
    return LineLayoutBoxModel(nullptr);
  }

  // Physical location of the top-left corner of the box in the containing
  // block.
  PhysicalOffset PhysicalLocation() const;

  // TODO(szager): The Rect versions should return a rect, not modify the
  // argument.
  void FlipForWritingMode(LayoutRect&) const;
  LayoutPoint FlipForWritingMode(const LayoutPoint&) const;

  // Returns trus if it is known that this box has no layout or visual
  // overflow. This is used as a fast-path to skip expensive overflow
  // recalc.
  bool KnownToHaveNoOverflow() const {
    return bitfields_.KnownToHaveNoOverflow();
  }
  void ClearKnownToHaveNoOverflow();

  bool DirOverride() const { return bitfields_.DirOverride(); }
  void SetDirOverride(bool dir_override) {
    bitfields_.SetDirOverride(dir_override);
  }

  // Set all LineLayoutItems in the inline box subtree should do full paint
  // invalidation and clear the first line style cache.
  void SetShouldDoFullPaintInvalidationForFirstLine();

#define ADD_BOOLEAN_BITFIELD(field_name_, MethodNameBase)               \
 public:                                                                \
  bool MethodNameBase() const { return field_name_; }                   \
  void Set##MethodNameBase(bool new_value) { field_name_ = new_value; } \
                                                                        \
 private:                                                               \
  unsigned field_name_ : 1

  class InlineBoxBitfields {
    DISALLOW_NEW();

   public:
    InlineBoxBitfields(bool first_line = false,
                       bool constructed = false,
                       bool dirty = false,
                       bool extracted = false,
                       bool is_horizontal = true)
        : first_line_(first_line),
          constructed_(constructed),
          bidi_embedding_level_(0),
          dirty_(dirty),
          extracted_(extracted),
          has_virtual_logical_height_(false),
          is_horizontal_(is_horizontal),
          ends_with_break_(false),
          can_have_leading_expansion_(false),
          known_to_have_no_overflow_(true),
          has_ellipsis_box_or_hyphen_(false),
          dir_override_(false),
          is_text_(false),
          expansion_(0) {}

    // Some of these bits are actually for subclasses and moved here to compact
    // the structures.
    // for this class
    ADD_BOOLEAN_BITFIELD(first_line_, FirstLine);
    ADD_BOOLEAN_BITFIELD(constructed_, Constructed);

   private:
    // The maximium bidi level is 62:
    // http://unicode.org/reports/tr9/#Explicit_Levels_and_Directions
    unsigned bidi_embedding_level_ : 6;

   public:
    unsigned char BidiEmbeddingLevel() const { return bidi_embedding_level_; }
    void SetBidiEmbeddingLevel(unsigned char bidi_embedding_level) {
      bidi_embedding_level_ = bidi_embedding_level;
    }

    ADD_BOOLEAN_BITFIELD(dirty_, Dirty);
    ADD_BOOLEAN_BITFIELD(extracted_, Extracted);
    ADD_BOOLEAN_BITFIELD(has_virtual_logical_height_, HasVirtualLogicalHeight);
    ADD_BOOLEAN_BITFIELD(is_horizontal_, IsHorizontal);
    // for RootInlineBox
    ADD_BOOLEAN_BITFIELD(ends_with_break_,
                         EndsWithBreak);  // Whether the line ends with a <br>.
    // shared between RootInlineBox and InlineTextBox
    ADD_BOOLEAN_BITFIELD(can_have_leading_expansion_, CanHaveLeadingExpansion);

    // This boolean will never be set if there is potential for overflow, but it
    // will be eagerly cleared in the opposite case. As such, it's a
    // conservative tracking of the absence of overflow.
    //
    // For whether we have overflow, callers should use |overflow_| on
    // InlineFlowBox.
    ADD_BOOLEAN_BITFIELD(known_to_have_no_overflow_, KnownToHaveNoOverflow);
    ADD_BOOLEAN_BITFIELD(has_ellipsis_box_or_hyphen_, HasEllipsisBoxOrHyphen);
    // for InlineTextBox
    ADD_BOOLEAN_BITFIELD(dir_override_, DirOverride);
    // Whether or not this object represents text with a non-zero height.
    // Includes non-image list markers, text boxes.
    ADD_BOOLEAN_BITFIELD(is_text_, IsText);

   private:
    unsigned expansion_ : 12;  // for justified text

   public:
    signed Expansion() const { return expansion_; }
    void SetExpansion(signed expansion) { expansion_ = expansion; }
  };
#undef ADD_BOOLEAN_BITFIELD

 private:
  void SetLineLayoutItemShouldDoFullPaintInvalidationIfNeeded();

  InlineBox* next_;  // The next element on the same line as us.
  InlineBox* prev_;  // The previous element on the same line as us.

  InlineFlowBox* parent_;  // The box that contains us.
  LineLayoutItem line_layout_item_;

 protected:
  // For RootInlineBox
  bool EndsWithBreak() const { return bitfields_.EndsWithBreak(); }
  void SetEndsWithBreak(bool ends_with_break) {
    bitfields_.SetEndsWithBreak(ends_with_break);
  }
  bool HasEllipsisBox() const { return bitfields_.HasEllipsisBoxOrHyphen(); }
  void SetHasEllipsisBox(bool has_ellipsis_box) {
    bitfields_.SetHasEllipsisBoxOrHyphen(has_ellipsis_box);
  }

  // For InlineTextBox
  bool HasHyphen() const { return bitfields_.HasEllipsisBoxOrHyphen(); }
  void SetHasHyphen(bool has_hyphen) {
    bitfields_.SetHasEllipsisBoxOrHyphen(has_hyphen);
  }
  bool CanHaveLeadingExpansion() const {
    return bitfields_.CanHaveLeadingExpansion();
  }
  void SetCanHaveLeadingExpansion(bool can_have_leading_expansion) {
    bitfields_.SetCanHaveLeadingExpansion(can_have_leading_expansion);
  }
  signed Expansion() { return bitfields_.Expansion(); }
  void SetExpansion(signed expansion) { bitfields_.SetExpansion(expansion); }

  // For InlineFlowBox and InlineTextBox
  bool Extracted() const { return bitfields_.Extracted(); }

  LayoutPoint location_;
  LayoutUnit logical_width_;

 private:
  InlineBoxBitfields bitfields_;

#if DCHECK_IS_ON()
  bool has_bad_parent_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(InlineBox);
};

#if !DCHECK_IS_ON()
inline InlineBox::~InlineBox() {}
#endif

#if DCHECK_IS_ON()
inline void InlineBox::SetHasBadParent() {
  has_bad_parent_ = true;
}
#endif

#define DEFINE_INLINE_BOX_TYPE_CASTS(typeName)                     \
  DEFINE_TYPE_CASTS(typeName, InlineBox, box, box->Is##typeName(), \
                    box.Is##typeName())

// Allow equality comparisons of InlineBox's by reference or pointer,
// interchangeably.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(InlineBox)

// TODO(layout-dev): Once LayoutNG supports inline layout, we should remove
// |CanUseInlineBox()|.
bool CanUseInlineBox(const LayoutObject&);

}  // namespace blink

#if DCHECK_IS_ON()
// Outside the blink namespace for ease of invocation from gdb.
void showTree(const blink::InlineBox*);
void showLineTree(const blink::InlineBox*);
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_BOX_H_
