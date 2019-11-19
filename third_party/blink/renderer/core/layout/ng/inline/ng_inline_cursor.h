// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_

#include <unicode/ubidi.h>

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_offset.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

class ComputedStyle;
class LayoutBlockFlow;
class LayoutInline;
class LayoutObject;
class LayoutUnit;
class NGFragmentItem;
class NGFragmentItems;
class NGInlineBreakToken;
class NGPaintFragment;
class NGPhysicalBoxFragment;
class Node;
class ShapeResultView;
enum class NGStyleVariant;
struct PhysicalOffset;
struct PhysicalRect;
struct PhysicalSize;

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
  using ItemsSpan = base::span<const std::unique_ptr<NGFragmentItem>>;

  explicit NGInlineCursor(const LayoutBlockFlow& block_flow);
  explicit NGInlineCursor(const NGFragmentItems& items);
  explicit NGInlineCursor(const NGFragmentItems& fragment_items,
                          ItemsSpan items);
  explicit NGInlineCursor(const NGPaintFragment& root_paint_fragment);
  NGInlineCursor(const NGInlineCursor& other);

  // Creates an |NGInlineCursor| without the root. Even when callers don't know
  // the root of the inline formatting context, this cursor can |MoveTo()|
  // specific |LayoutObject|.
  NGInlineCursor();

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

  // Returns true if cursor is out of fragment tree, e.g. before first fragment
  // or after last fragment in tree.
  bool IsNull() const { return !current_item_ && !current_paint_fragment_; }
  bool IsNotNull() const { return !IsNull(); }
  explicit operator bool() const { return !IsNull(); }

  // True if fragment at the current position can have children.
  bool CanHaveChildren() const;

  // True if fragment at the current position has children.
  bool HasChildren() const;

  // Returns a new |NGInlineCursor| whose root is the current item. The returned
  // cursor can traverse descendants of the current item. If the current item
  // has no children, returns an empty cursor.
  NGInlineCursor CursorForDescendants() const;

  // True if current position has soft wrap to next line. It is error to call
  // other than line.
  bool HasSoftWrapToNextLine() const;

  // True if the current position is a atomic inline. It is error to call at
  // end.
  bool IsAtomicInline() const;

  // True if the current position is before soft line break. It is error to call
  // at end.
  bool IsBeforeSoftLineBreak() const;

  // True if the current position is an ellipsis. It is error to call at end.
  bool IsEllipsis() const;

  // True if the current position is an empty line box. It is error to call
  // other then line box.
  bool IsEmptyLineBox() const;

  // True if the current position is a generatd text. It is error to call at
  // end.
  bool IsGeneratedText() const;

  // True if fragment is |NGFragmentItem::kGeneratedText| or
  // |NGPhysicalTextFragment::kGeneratedText|.
  // TODO(yosin): We should rename |IsGeneratedTextType()| to another name.
  bool IsGeneratedTextType() const;

  // True if the current position is hidden for paint. It is error to call at
  // end.
  bool IsHiddenForPaint() const;

  // True if the current position's writing mode in style is horizontal.
  bool IsHorizontal() const;

  // True if the current position is text or atomic inline box.
  // Note: Because of this function is used for caret rect, hit testing, etc,
  // this function returns false for hidden for paint, text overflow ellipsis,
  // and line break hyphen.
  bool IsInlineLeaf() const;

  // True if the current position is a line box. It is error to call at end.
  bool IsLineBox() const;

  // True if the current position is a line break. It is error to call at end.
  bool IsLineBreak() const;

  // True if the current position is a list marker.
  bool IsListMarker() const;

  // True if the current position is a text. It is error to call at end.
  bool IsText() const;

  // |Current*| functions return an object for the current position.
  const NGFragmentItem* CurrentItem() const { return current_item_; }
  const NGPaintFragment* CurrentPaintFragment() const {
    return current_paint_fragment_;
  }
  // Returns text direction of current line. It is error to call at other than
  // line.
  TextDirection CurrentBaseDirection() const;
  const NGPhysicalBoxFragment* CurrentBoxFragment() const;
  const LayoutObject* CurrentLayoutObject() const;
  Node* CurrentNode() const;

  // Returns bidi level of current position. It is error to call other than
  // text and atomic inline. It is also error to call |IsGeneratedTextType()|.
  UBiDiLevel CurrentBidiLevel() const;

  // Returns text direction of current text or atomic inline. It is error to
  // call at other than text or atomic inline. Note: <span> doesn't have
  // reserved direction.
  TextDirection CurrentResolvedDirection() const;
  const ComputedStyle& CurrentStyle() const;

  // InkOverflow of itself, including contents if they contribute to the ink
  // overflow of this object (e.g. when not clipped,) in the local coordinate.
  const PhysicalRect CurrentInkOverflow() const;
  // The offset relative to the root of the inline formatting context.
  const PhysicalOffset CurrentOffset() const;
  const PhysicalRect CurrentRect() const;
  const PhysicalSize CurrentSize() const;

  // Returns start/end of offset in text content of current text fragment.
  // It is error when this cursor doesn't point to text fragment.
  NGTextOffset CurrentTextOffset() const;
  unsigned CurrentTextStartOffset() const { return CurrentTextOffset().start; }
  unsigned CurrentTextEndOffset() const { return CurrentTextOffset().end; }

  // Returns text of the current position. It is error to call other than
  // text.
  StringView CurrentText() const;

  // Returns |ShapeResultView| of the current position. It is error to call
  // other than text.
  const ShapeResultView* CurrentTextShapeResult() const;

  // The layout box of text in (start, end) range in local coordinate.
  // Start and end offsets must be between |CurrentTextStartOffset()| and
  // |CurrentTextEndOffset()|. It is error to call other than text.
  PhysicalRect CurrentLocalRect(unsigned start_offset,
                                unsigned end_offset) const;

  // Relative to fragment of the current position. It is error to call other
  // than text.
  LayoutUnit InlinePositionForOffset(unsigned offset) const;

  // Returns a point at the visual start/end of the line.
  // Encapsulates the handling of text direction and writing mode.
  PhysicalOffset LineStartPoint() const;
  PhysicalOffset LineEndPoint() const;

  // Converts the given point, relative to the fragment itself, into a position
  // in DOM tree.
  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const;

  //
  // Functions to move the current position.
  //

  // Move the current position at |cursor|. Unlinke copy constrcutr, this
  // function doesn't copy root. Note: The current position in |cursor|
  // should be part of |this| cursor.
  void MoveTo(const NGInlineCursor& cursor);

  // Move the current posint at |paint_fragment|.
  void MoveTo(const NGPaintFragment& paint_fragment);

  // Move to first |NGFragmentItem| or |NGPaintFragment| associated to
  // |layout_object|. When |layout_object| has no associated fragments, this
  // cursor points nothing.
  void MoveTo(const LayoutObject& layout_object);

  // Move to containing line box. It is error if the current position is line.
  void MoveToContainingLine();

  // Move to first child of current container box. If the current position is
  // at fragment without children, this cursor points nothing.
  // See also |TryToMoveToFirstChild()|.
  void MoveToFirstChild();

  // Move to first logical leaf of current line box. If current line box has
  // no children, curosr becomes null.
  void MoveToFirstLogicalLeaf();

  // Move to last child of current container box. If the current position is
  // at fragment without children, this cursor points nothing.
  // See also |TryToMoveToFirstChild()|.
  void MoveToLastChild();

  // Move to last logical leaf of current line box. If current line box has
  // no children, curosr becomes null.
  void MoveToLastLogicalLeaf();

  // Move the current position to the next fragment in pre-order DFS. When
  // the current position is at last fragment, this cursor points nothing.
  void MoveToNext();

  // Move the current position to next fragment on same layout object.
  void MoveToNextForSameLayoutObject();

  // Move the current position to next line. It is error to call other than line
  // box.
  void MoveToNextLine();

  // Move the current position to next sibling fragment.
  void MoveToNextSibling();

  // Same as |MoveToNext| except that this skips children even if they exist.
  void MoveToNextSkippingChildren();

  // Move the current to next/previous inline leaf.
  void MoveToNextInlineLeaf();
  void MoveToNextInlineLeafIgnoringLineBreak();
  void MoveToPreviousInlineLeaf();
  void MoveToPreviousInlineLeafIgnoringLineBreak();

  // Move the cursor position to previous fragment in pre-order DFS.
  void MoveToPrevious();

  // Move the current position to previous line. It is error to call other than
  // line box.
  void MoveToPreviousLine();

  // Returns true if the current position moves to first child.
  bool TryToMoveToFirstChild();

  // Returns true if the current position moves to last child.
  bool TryToMoveToLastChild();

  // TODO(kojii): Add more variations as needed, NextSibling,
  // NextSkippingChildren, Previous, etc.

 private:
  // Returns break token for line box. It is error to call other than line box.
  const NGInlineBreakToken& CurrentInlineBreakToken() const;

  // Returns style variant of the current position.
  NGStyleVariant CurrentStyleVariant() const;
  bool UsesFirstLineStyle() const;

  // True if current position is descendant or self of |layout_object|.
  // Note: This function is used for moving cursor in culled inline boxes.
  bool IsInclusiveDescendantOf(const LayoutObject& layout_object) const;

  // True if the current position is a last line in inline block. It is error
  // to call at end or the current position is not line.
  bool IsLastLineInInlineBlock() const;

  // Make the current position points nothing, e.g. cursor moves over start/end
  // fragment, cursor moves to first/last child to parent has no children.
  void MakeNull();

  // Move the cursor position to the first fragment in tree.
  void MoveToFirst();

  // Same as |MoveTo()| but not support culled inline.
  void InternalMoveTo(const LayoutObject& layout_object);

  void SetRoot(const NGFragmentItems& items);
  void SetRoot(const NGFragmentItems& fragment_items, ItemsSpan items);
  void SetRoot(const NGPaintFragment& root_paint_fragment);
  void SetRoot(const LayoutBlockFlow& block_flow);

  void MoveToItem(const ItemsSpan::iterator& iter);
  void MoveToNextItem();
  void MoveToNextItemSkippingChildren();
  void MoveToNextSiblingItem();
  void MoveToPreviousItem();

  void MoveToParentPaintFragment();
  void MoveToNextPaintFragment();
  void MoveToNextSiblingPaintFragment();
  void MoveToNextPaintFragmentSkippingChildren();
  void MoveToPreviousPaintFragment();
  void MoveToPreviousSiblingPaintFragment();

  ItemsSpan items_;
  ItemsSpan::iterator item_iter_;
  const NGFragmentItem* current_item_ = nullptr;
  const NGFragmentItems* fragment_items_ = nullptr;

  const NGPaintFragment* root_paint_fragment_ = nullptr;
  const NGPaintFragment* current_paint_fragment_ = nullptr;

  // Used in |MoveToNextForSameLayoutObject()| to support culled inline.
  const LayoutInline* layout_inline_ = nullptr;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGInlineCursor&);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGInlineCursor*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_
