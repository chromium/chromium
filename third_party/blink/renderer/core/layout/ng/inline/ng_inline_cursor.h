// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_

#include <unicode/ubidi.h>

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_offset.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

class ComputedStyle;
class DisplayItemClient;
class LayoutBlockFlow;
class LayoutInline;
class LayoutObject;
class LayoutUnit;
class NGFragmentItem;
class NGFragmentItems;
class NGInlineBackwardCursor;
class NGInlineBreakToken;
class NGInlineCursor;
class NGPaintFragment;
class NGPhysicalBoxFragment;
class Node;
class ShapeResultView;
enum class NGStyleVariant;
struct PhysicalOffset;
struct PhysicalRect;
struct PhysicalSize;

// Represents a position of |NGInlineCursor|. This class:
// 1. Provides properties for the current position.
// 2. Allows to save |Current()|, and can move back later. Moving to |Position|
// is faster than moving to |NGFragmentItem|.
class CORE_EXPORT NGInlineCursorPosition {
 public:
  using ItemsSpan = NGFragmentItems::Span;

  const NGPaintFragment* PaintFragment() const { return paint_fragment_; }
  const NGFragmentItem* Item() const { return item_; }
  const NGFragmentItem* operator->() const { return item_; }
  const NGFragmentItem& operator*() const { return *item_; }

  operator bool() const { return paint_fragment_ || item_; }

  bool operator==(const NGInlineCursorPosition& other) const {
    return paint_fragment_ == other.paint_fragment_ && item_ == other.item_;
  }
  bool operator!=(const NGInlineCursorPosition& other) const {
    return !operator==(other);
  }

  // True if the current position is a text. It is error to call at end.
  bool IsText() const;

  // True if the current position is a generatd text. It is error to call at
  // end. This includes both style-generated (e.g., `content` property, see
  // |IsStyleGenerated()|) and layout-generated (hyphens and ellipsis, see
  // |IsLayoutGeneratedText()|.)
  bool IsGeneratedText() const;

  // True if fragment is layout-generated (hyphens and ellipsis.)
  bool IsLayoutGeneratedText() const;

  // True if the current position is a line break. It is error to call at end.
  bool IsLineBreak() const;

  // True if the current position is an ellipsis. It is error to call at end.
  bool IsEllipsis() const;

  // True if the current position is a line box. It is error to call at end.
  bool IsLineBox() const;

  // True if the current position is an empty line box. It is error to call
  // other then line box.
  bool IsEmptyLineBox() const;

  // True if the current position is an inline box. It is error to call at end.
  bool IsInlineBox() const;

  // True if the current position is an atomic inline. It is error to call at
  // end.
  bool IsAtomicInline() const;

  // True if the current position is a list marker.
  bool IsListMarker() const;

  // True if the current position is hidden for paint. It is error to call at
  // end.
  bool IsHiddenForPaint() const;

  // |ComputedStyle| and related functions.
  NGStyleVariant StyleVariant() const;
  bool UsesFirstLineStyle() const;
  const ComputedStyle& Style() const;

  // Functions to get corresponding objects for this position.
  const NGPhysicalBoxFragment* BoxFragment() const;
  const LayoutObject* GetLayoutObject() const;
  LayoutObject* GetMutableLayoutObject() const;
  const Node* GetNode() const;
  const DisplayItemClient* GetDisplayItemClient() const;
  const DisplayItemClient* GetSelectionDisplayItemClient() const;
  wtf_size_t FragmentId() const;

  // True if fragment at the current position can have children.
  bool CanHaveChildren() const;

  // True if fragment at the current position has children.
  bool HasChildren() const;

  // Returns break token for line box. It is error to call other than line box.
  const NGInlineBreakToken* InlineBreakToken() const;

  // The offset relative to the root of the inline formatting context.
  const PhysicalRect RectInContainerBlock() const;
  const PhysicalOffset OffsetInContainerBlock() const;
  const PhysicalSize Size() const;

  // InkOverflow of itself, including contents if they contribute to the ink
  // overflow of this object (e.g. when not clipped,) in the local coordinate.
  const PhysicalRect InkOverflow() const;
  const PhysicalRect SelfInkOverflow() const;

  // Returns start/end of offset in text content of current text fragment.
  // It is error when this cursor doesn't point to text fragment.
  NGTextOffset TextOffset() const;
  unsigned TextStartOffset() const { return TextOffset().start; }
  unsigned TextEndOffset() const { return TextOffset().end; }

  // Returns text of the current position. It is error to call other than
  // text.
  StringView Text(const NGInlineCursor& cursor) const;

  // Returns |ShapeResultView| of the current position. It is error to call
  // other than text.
  const ShapeResultView* TextShapeResult() const;

  // Returns bidi level of current position. It is error to call other than
  // text and atomic inline. It is also error to call |IsGeneratedTextType()|.
  UBiDiLevel BidiLevel() const;
  // Returns text direction of current text or atomic inline. It is error to
  // call at other than text or atomic inline. Note: <span> doesn't have
  // reserved direction.
  TextDirection ResolvedDirection() const;
  // Returns text direction of current line. It is error to call at other than
  // line.
  TextDirection BaseDirection() const;

  TextDirection ResolvedOrBaseDirection() const {
    return IsLineBox() ? BaseDirection() : ResolvedDirection();
  }

  // True if the current position is text or atomic inline box.
  // Note: Because of this function is used for caret rect, hit testing, etc,
  // this function returns false for hidden for paint, text overflow ellipsis,
  // and line break hyphen.
  bool IsInlineLeaf() const;

  // True if current position has soft wrap to next line. It is error to call
  // other than line.
  bool HasSoftWrapToNextLine() const;

  // Returns a point at the visual start/end of the line. (0, 0) is left-top of
  // the line.
  // Encapsulates the handling of text direction and writing mode.
  PhysicalOffset LineStartPoint() const;
  PhysicalOffset LineEndPoint() const;

  // LogicalRect/PhysicalRect conversions
  // |logical_rect| and |physical_rect| are converted with |Size()| as
  // "outer size".
  LogicalRect ConvertChildToLogical(const PhysicalRect& physical_rect) const;
  PhysicalRect ConvertChildToPhysical(const LogicalRect& logical_rect) const;

 private:
  void Set(const ItemsSpan::iterator& iter) {
    DCHECK(!paint_fragment_);
    item_iter_ = iter;
    item_ = &*iter;
  }

  void Clear() {
    paint_fragment_ = nullptr;
    item_ = nullptr;
  }

  // True if current position is part of culled inline box |layout_inline|.
  bool IsPartOfCulledInlineBox(const LayoutInline& layout_inline) const;

  const NGPaintFragment* paint_fragment_ = nullptr;
  const NGFragmentItem* item_ = nullptr;
  ItemsSpan::iterator item_iter_;

  friend class NGInlineBackwardCursor;
  friend class NGInlineCursor;
};

// This class traverses fragments in an inline formatting context.
//
// When constructed, the initial position is empty. Call |MoveToNext()| to move
// to the first fragment.
//
// TODO(kojii): |NGPaintFragment| should be gone when |NGPaintFragment| is
// deprecated and all its uses are removed.
class CORE_EXPORT NGInlineCursor {
  STACK_ALLOCATED();

 public:
  using ItemsSpan = NGFragmentItems::Span;

  explicit NGInlineCursor(const LayoutBlockFlow& block_flow);
  explicit NGInlineCursor(const NGFragmentItems& items);
  explicit NGInlineCursor(const NGFragmentItems& fragment_items,
                          ItemsSpan items);
  explicit NGInlineCursor(const NGPaintFragment& root_paint_fragment);
  explicit NGInlineCursor(const NGInlineBackwardCursor& backward_cursor);
  NGInlineCursor(const NGInlineCursor& other) = default;

  // Creates an |NGInlineCursor| without the root. Even when callers don't know
  // the root of the inline formatting context, this cursor can |MoveTo()|
  // specific |LayoutObject|.
  NGInlineCursor() = default;

  bool operator==(const NGInlineCursor& other) const;
  bool operator!=(const NGInlineCursor& other) const {
    return !operator==(other);
  }

  bool IsItemCursor() const { return fragment_items_; }
  bool IsPaintFragmentCursor() const { return root_paint_fragment_; }

  // True if this cursor has the root to traverse. Only the default constructor
  // creates a cursor without the root.
  bool HasRoot() const { return IsItemCursor() || IsPaintFragmentCursor(); }

  const NGFragmentItems& Items() const {
    DCHECK(fragment_items_);
    return *fragment_items_;
  }

  // Returns the |LayoutBlockFlow| containing this cursor.
  const LayoutBlockFlow* GetLayoutBlockFlow() const;

  //
  // Functions to query the current position.
  //
  const NGInlineCursorPosition& Current() const { return current_; }

  // Returns true if cursor is out of fragment tree, e.g. before first fragment
  // or after last fragment in tree.
  bool IsNull() const { return !Current(); }
  bool IsNotNull() const { return Current(); }
  operator bool() const { return Current(); }

  // True if |Current()| is at the first fragment. See |MoveToFirst()|.
  bool IsAtFirst() const;

  // Returns a new |NGInlineCursor| whose root is the current item. The returned
  // cursor can traverse descendants of the current item. If the current item
  // has no children, returns an empty cursor.
  NGInlineCursor CursorForDescendants() const;

  // If |this| is created by |CursorForDescendants()| to traverse parts of an
  // inline formatting context, expand the traversable range to the containing
  // |LayoutBlockFlow|. Does nothing if |this| is for an inline formatting
  // context.
  void ExpandRootToContainingBlock();

  // True if the current position is before soft line break. It is error to call
  // at end.
  bool IsBeforeSoftLineBreak() const;

  // |Current*| functions return an object for the current position.
  const NGFragmentItem* CurrentItem() const { return Current().Item(); }
  const NGPaintFragment* CurrentPaintFragment() const {
    return Current().PaintFragment();
  }
  LayoutObject* CurrentMutableLayoutObject() const {
    return Current().GetMutableLayoutObject();
  }

  // Returns text of the current position. It is error to call other than
  // text.
  StringView CurrentText() const { return Current().Text(*this); }

  // The layout box of text in (start, end) range in local coordinate.
  // Start and end offsets must be between |CurrentTextStartOffset()| and
  // |CurrentTextEndOffset()|. It is error to call other than text.
  PhysicalRect CurrentLocalRect(unsigned start_offset,
                                unsigned end_offset) const;

  // Relative to fragment of the current position. It is error to call other
  // than text.
  LayoutUnit InlinePositionForOffset(unsigned offset) const;

  // Converts the given point, relative to the fragment itself, into a position
  // in DOM tree within the range of |this|. This variation ignores the inline
  // offset, and snaps to the nearest line in the block direction.
  PositionWithAffinity PositionForPointInInlineFormattingContext(
      const PhysicalOffset& point,
      const NGPhysicalBoxFragment& container);
  // Find the |Position| in the line box |Current()| points to. This variation
  // ignores the block offset, and snaps to the nearest item in inline
  // direction.
  PositionWithAffinity PositionForPointInInlineBox(
      const PhysicalOffset& point) const;

  // Returns |PositionWithAffinity| in current position at x-coordinate of
  // |point_in_container| for horizontal writing mode, or y-coordinate of
  // |point_in_container| for vertical writing mode.
  // Note: Even if |point_in_container| is outside of an item of current
  // position, this function returns boundary position of an item.
  // Note: This function is used for locating caret at same x/y-coordinate as
  // previous caret after line up/down.
  PositionWithAffinity PositionForPointInChild(
      const PhysicalOffset& point_in_container) const;

  // Returns first/last position of |this| line. |this| should be line box.
  PositionWithAffinity PositionForStartOfLine() const;
  PositionWithAffinity PositionForEndOfLine() const;

  //
  // Functions to move the current position.
  //
  void MoveTo(const NGInlineCursorPosition& position);

  // Move the current position at |fragment_item|.
  void MoveTo(const NGFragmentItem& fragment_item);

  // Move the current position at |cursor|. Unlinke copy constrcutr, this
  // function doesn't copy root. Note: The current position in |cursor|
  // should be part of |this| cursor.
  void MoveTo(const NGInlineCursor& cursor);

  // Move the current posint at |paint_fragment|.
  void MoveTo(const NGPaintFragment& paint_fragment);
  void MoveTo(const NGPaintFragment* paint_fragment);

  // Move to containing line box. It is error if the current position is line.
  void MoveToContainingLine();

  // Move to first child of current container box. If the current position is
  // at fragment without children, this cursor points nothing.
  // See also |TryToMoveToFirstChild()|.
  void MoveToFirstChild();

  // Move to the first line.
  void MoveToFirstLine();

  // Move to first logical leaf of current line box. If current line box has
  // no children, curosr becomes null.
  void MoveToFirstLogicalLeaf();

  // Move to last child of current container box. If the current position is
  // at fragment without children, this cursor points nothing.
  // See also |TryToMoveToFirstChild()|.
  void MoveToLastChild();

  // Move to the last line item. If there are no line items, the cursor becomes
  // null.
  void MoveToLastLine();

  // Move to last logical leaf of current line box. If current line box has
  // no children, curosr becomes null.
  void MoveToLastLogicalLeaf();

  // Move the current position to the next fragment in pre-order DFS. When
  // the current position is at last fragment, this cursor points nothing.
  void MoveToNext();

  // Move the current position to next line. It is error to call other than line
  // box.
  void MoveToNextLine();

  // Same as |MoveToNext| except that this skips children even if they exist.
  void MoveToNextSkippingChildren();

  // Move the current to next/previous inline leaf.
  void MoveToNextInlineLeaf();
  void MoveToNextInlineLeafIgnoringLineBreak();
  void MoveToPreviousInlineLeaf();
  void MoveToPreviousInlineLeafIgnoringLineBreak();

  // Move the current position to next/previous inline leaf item on line.
  // Note: If the current position isn't leaf item, this function moves the
  // current position to leaf item then moves to next/previous leaf item. This
  // behavior doesn't match |MoveTo{Next,Previous}InlineLeaf()|, but AX requires
  // this. See AccessibilityLayoutTest.NextOnLine
  void MoveToNextInlineLeafOnLine();
  void MoveToPreviousInlineLeafOnLine();

  // Move the cursor position to previous fragment in pre-order DFS.
  void MoveToPrevious();

  // Move the current position to previous line. It is error to call other than
  // line box.
  void MoveToPreviousLine();

  // Returns true if the current position moves to first child.
  bool TryToMoveToFirstChild();

  // Returns true if the current position moves to first inline leaf child.
  bool TryToMoveToFirstInlineLeafChild();

  // Returns true if the current position moves to last child.
  bool TryToMoveToLastChild();

  //
  // Moving across fragmentainers.
  //
  // When rooted at |LayoutBlockFlow|, |this| can move the current position
  // across fragmentainers. Other root objects (e.g. |NGFragmentItems|) can
  // contain only one fragmentainer that such cursors cannot move to different
  // fragmentainers. See |CanMoveAcrossFragmentainer()|.
  //
  // However, |MoveToNext| etc. does not move the current position across
  // fragmentainers. Use following functions when moving to different
  // fragmentainers.

  // Move to the first item of the first fragmentainer.
  void MoveToFirstIncludingFragmentainer();

  // Move to the next fragmentainer. Valid when |CanMoveAcrossFragmentainer|.
  void MoveToNextFragmentainer();

  // Same as |MoveToNext|, except this moves to the next fragmentainer if
  // |Current| is at the end of a fragmentainer.
  void MoveToNextIncludingFragmentainer();

  //
  // Functions to enumerate fragments for a |LayoutObject|.
  //

  // Move to first |NGFragmentItem| or |NGPaintFragment| associated to
  // |layout_object|. When |layout_object| has no associated fragments, this
  // cursor points nothing.
  void MoveTo(const LayoutObject& layout_object);

  // Same as |MoveTo|, except that this enumerates fragments for descendants
  // if |layout_object| is a culled inline.
  //
  // Note, for a culled inline, fragments may not be in the visual order in
  // the inline direction if RTL or mixed bidi for a performance reason.
  void MoveToIncludingCulledInline(const LayoutObject& layout_object);

  // Move the current position to next fragment on same layout object.
  void MoveToNextForSameLayoutObject();

  // Move the current position to the last fragment on same layout object.
  void MoveToLastForSameLayoutObject();

#if DCHECK_IS_ON()
  void CheckValid(const NGInlineCursorPosition& position) const;
#else
  void CheckValid(const NGInlineCursorPosition&) const {}
#endif

 private:
  // Returns true if |this| is only for a part of an inline formatting context;
  // in other words, if |this| is created by |CursorForDescendants|.
  bool IsDescendantsCursor() const {
    if (fragment_items_)
      return !fragment_items_->Equals(items_);
    if (root_paint_fragment_)
      return root_paint_fragment_->Parent();
    return false;
  }
  bool CanMoveAcrossFragmentainer() const {
    return root_block_flow_ && IsItemCursor() && !IsDescendantsCursor();
  }

  // True if the current position is a last line in inline block. It is error
  // to call at end or the current position is not line.
  bool IsLastLineInInlineBlock() const;

  // Index conversions for |IsDescendantsCursor()|.
  wtf_size_t SpanBeginItemIndex() const;
  wtf_size_t SpanIndexFromItemIndex(unsigned index) const;

  // Make the current position points nothing, e.g. cursor moves over start/end
  // fragment, cursor moves to first/last child to parent has no children.
  void MakeNull() { current_.Clear(); }

  // Move the cursor position to the first fragment in tree.
  void MoveToFirst();

  void SetRoot(const NGFragmentItems& items);
  void SetRoot(const NGFragmentItems& fragment_items, ItemsSpan items);
  void SetRoot(const NGPaintFragment& root_paint_fragment);
  void SetRoot(const LayoutBlockFlow& block_flow);
  bool SetRoot(const LayoutBlockFlow& block_flow, wtf_size_t fragment_index);

  bool TrySetRootFragmentItems();

  void MoveToItem(const ItemsSpan::iterator& iter);
  void MoveToNextItem();
  void MoveToNextItemSkippingChildren();
  void MoveToPreviousItem();

  void MoveToParentPaintFragment();
  void MoveToNextPaintFragment();
  void MoveToNextSiblingPaintFragment();
  void MoveToNextPaintFragmentSkippingChildren();
  void MoveToPreviousPaintFragment();
  void MoveToPreviousSiblingPaintFragment();

  void SlowMoveToFirstFor(const LayoutObject& layout_object);
  void SlowMoveToNextForSameLayoutObject(const LayoutObject& layout_object);
  void SlowMoveToForIfNeeded(const LayoutObject& layout_object);

  // |MoveToNextForSameLayoutObject| that doesn't check |culled_inline_|.
  void MoveToNextForSameLayoutObjectExceptCulledInline();

  // A helper class to enumerate |LayoutObject|s that contribute to a culled
  // inline.
  class CulledInlineTraversal {
    STACK_ALLOCATED();

   public:
    CulledInlineTraversal() = default;

    const LayoutInline* GetLayoutInline() const { return layout_inline_; }

    explicit operator bool() const { return layout_inline_; }
    void Reset() { layout_inline_ = nullptr; }

    bool UseFragmentTree() const { return use_fragment_tree_; }
    void SetUseFragmentTree(const LayoutInline& layout_inline);

    // Returns first/next |LayoutObject| that contribute to |layout_inline|.
    const LayoutObject* MoveToFirstFor(const LayoutInline& layout_inline);
    const LayoutObject* MoveToNext();

   private:
    const LayoutObject* Find(const LayoutObject* child) const;

    const LayoutObject* current_object_ = nullptr;
    const LayoutInline* layout_inline_ = nullptr;
    bool use_fragment_tree_ = false;
  };

  void MoveToFirstForCulledInline(const LayoutInline& layout_inline);
  void MoveToNextForCulledInline();
  void MoveToNextCulledInlineDescendantIfNeeded();

  NGInlineCursorPosition current_;

  ItemsSpan items_;
  const NGFragmentItems* fragment_items_ = nullptr;

  const NGPaintFragment* root_paint_fragment_ = nullptr;

  CulledInlineTraversal culled_inline_;

  // Used to traverse multiple |NGFragmentItems| when block fragmented.
  const LayoutBlockFlow* root_block_flow_ = nullptr;
  wtf_size_t fragment_index_ = 0;
  wtf_size_t max_fragment_index_ = 0;

  friend class NGInlineBackwardCursor;
};

// This class provides the |MoveToPreviousSibling| functionality, but as a
// separate class because it consumes memory, and only rarely used.
class CORE_EXPORT NGInlineBackwardCursor {
  STACK_ALLOCATED();

 public:
  // |cursor| should be the first child of root or descendants, e.g. the first
  // item in |NGInlineCursor::items_|.
  explicit NGInlineBackwardCursor(const NGInlineCursor& cursor);

  const NGInlineCursorPosition& Current() const { return current_; }
  operator bool() const { return Current(); }

  NGInlineCursor CursorForDescendants() const;

  void MoveToPreviousSibling();

 private:
  NGInlineCursorPosition current_;
  const NGInlineCursor& cursor_;
  Vector<const NGPaintFragment*, 16> sibling_paint_fragments_;
  Vector<NGInlineCursor::ItemsSpan::iterator, 16> sibling_item_iterators_;
  wtf_size_t current_index_;

  friend class NGInlineCursor;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGInlineCursor&);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGInlineCursor*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_
