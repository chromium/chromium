/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2009, 2013 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_H_

#include <iterator>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_span.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class AbstractInlineTextBox;
class ContentCaptureManager;
class OffsetMapping;
struct InlineItemsData;
struct InlineItemSpan;

enum class OnlyWhitespaceOrNbsp : unsigned { kUnknown = 0, kNo = 1, kYes = 2 };

// LayoutText is the root class for anything that represents
// a text node (see core/dom/text.h).
//
// This is a common node in the tree so to the limit memory overhead,
// this class inherits directly from LayoutObject.
// Also this class is used by both CSS and SVG layouts so LayoutObject
// was a natural choice.
//
// The actual layout of text is handled by the containing inline
// (LayoutInline) or block (LayoutBlockFlow). They will invoke the Unicode
// Bidirectional Algorithm to break the text into actual lines.
// The result of layout is the line box tree, which represents lines
// on the screen. It is stored into m_firstTextBox and m_lastTextBox.
// To understand how lines are broken by the bidi algorithm, read e.g.
// LayoutBlockFlow::LayoutInlineChildren.
//
//
// This class implements the preferred logical widths computation
// for its underlying text. The widths are stored into min_width_
// and max_width_. They are computed lazily based on
// LayoutObjectBitfields::intrinsic_logical_widths_dirty_.
//
// The previous comment applies also for painting. See e.g.
// BlockFlowPainter::paintContents in particular the use of LineBoxListPainter.
class CORE_EXPORT LayoutText : public LayoutObject {
 public:
  // FIXME: If the node argument is not a Text node or the string argument is
  // not the content of the Text node, updating text-transform property
  // doesn't re-transform the string.
  LayoutText(Node*, String);

  void Trace(Visitor*) const override;

  static LayoutText* CreateEmptyAnonymous(Document&, const ComputedStyle*);

  static LayoutText* CreateAnonymousForFormattedText(Document&,
                                                     const ComputedStyle*,
                                                     String);

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutText";
  }

  bool IsLayoutNGObject() const override {
    NOT_DESTROYED();
    return true;
  }

  bool IsTextFragment() const {
    NOT_DESTROYED();
    return is_text_fragment_;
  }
  virtual bool IsWordBreak() const;

  virtual String OriginalText() const;

  bool HasInlineFragments() const final;
  wtf_size_t FirstInlineFragmentItemIndex() const final;
  void ClearFirstInlineFragmentItemIndex() final;
  void SetFirstInlineFragmentItemIndex(wtf_size_t) final;

  const String& GetText() const {
    NOT_DESTROYED();
    return text_;
  }
  virtual unsigned TextStartOffset() const {
    NOT_DESTROYED();
    return 0;
  }
  virtual String PlainText() const;

  // Returns first letter part of |LayoutTextFragment|.
  virtual LayoutText* GetFirstLetterPart() const {
    NOT_DESTROYED();
    return nullptr;
  }

  void DirtyOrDeleteLineBoxesIfNeeded(bool full_layout);
  void DirtyLineBoxes();

  void AbsoluteQuads(Vector<gfx::QuadF>&,
                     MapCoordinatesFlags mode = 0) const final;
  void AbsoluteQuadsForRange(Vector<gfx::QuadF>&,
                             unsigned start_offset = 0,
                             unsigned end_offset = INT_MAX) const;
  gfx::RectF LocalBoundingBoxRectForAccessibility() const final;

  enum ClippingOption { kNoClipping, kClipToEllipsis };
  void LocalQuadsInFlippedBlocksDirection(Vector<gfx::QuadF>&,
                                          ClippingOption = kNoClipping) const;

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  bool Is8Bit() const {
    NOT_DESTROYED();
    return text_.Is8Bit();
  }
  const LChar* Characters8() const {
    NOT_DESTROYED();
    return text_.Characters8();
  }
  const UChar* Characters16() const {
    NOT_DESTROYED();
    return text_.Characters16();
  }
  bool HasEmptyText() const {
    NOT_DESTROYED();
    return text_.empty();
  }
  UChar CharacterAt(unsigned) const;
  UChar UncheckedCharacterAt(unsigned) const;
  UChar operator[](unsigned i) const {
    NOT_DESTROYED();
    return UncheckedCharacterAt(i);
  }
  UChar32 CodepointAt(unsigned) const;
  unsigned TextLength() const {
    NOT_DESTROYED();
    return text_.length();
  }  // non virtual implementation of length()
  bool ContainsOnlyWhitespace(unsigned from, unsigned len) const;

  // Get characters after whitespace collapsing was applied. Returns 0 if there
  // were no characters left. If whitespace collapsing is disabled (i.e.
  // white-space: pre), returns characters without whitespace collapsing.
  UChar32 FirstCharacterAfterWhitespaceCollapsing() const;
  UChar32 LastCharacterAfterWhitespaceCollapsing() const;

  virtual PhysicalRect PhysicalLinesBoundingBox() const;

  // Returns the bounding box of visual overflow rects of all line boxes,
  // in containing block's physical coordinates with flipped blocks direction.
  PhysicalRect VisualOverflowRect() const;

  void InvalidateVisualOverflow();

  PhysicalOffset FirstLineBoxTopLeft() const;

  void SetTextIfNeeded(String);
  void ForceSetText(String);
  void SetTextWithOffset(String, unsigned offset, unsigned len);
  void SetTextInternal(String);

  virtual void TransformText();

  PhysicalRect LocalSelectionVisualRect() const final;
  PhysicalRect LocalCaretRect(
      int caret_offset,
      LayoutUnit* extra_width_to_end_of_line = nullptr) const override;

  // Compute the rect and offset of text boxes for this LayoutText.
  struct TextBoxInfo {
    PhysicalRect local_rect;
    unsigned dom_start_offset;
    unsigned dom_length;
  };
  Vector<TextBoxInfo> GetTextBoxInfo() const;

  // Returns the Position in DOM that corresponds to the given offset in the
  // |text_| string.
  // TODO(layout-dev): Fix it when text-transform changes text length.
  virtual Position PositionForCaretOffset(unsigned) const;

  // Returns the offset in the |text_| string that corresponds to the given
  // position in DOM; Returns nullopt is the position is not in this LayoutText.
  // TODO(layout-dev): Fix it when text-transform changes text length.
  virtual absl::optional<unsigned> CaretOffsetForPosition(
      const Position&) const;

  // Returns true if the offset (0-based in the |text_| string) is next to a
  // non-collapsed non-linebreak character, or before a forced linebreak (<br>,
  // or segment break in node with style white-space: pre/pre-line/pre-wrap).
  // TODO(editing-dev): The behavior is introduced by crrev.com/e3eb4e in
  // InlineTextBox::ContainsCaretOffset(). Try to understand it.
  bool ContainsCaretOffset(int) const;

  // Return true if the offset (0-based in the |text_| string) is before/after a
  // non-collapsed character in this LayoutText, respectively.
  bool IsBeforeNonCollapsedCharacter(unsigned) const;
  bool IsAfterNonCollapsedCharacter(unsigned) const;

  virtual int CaretMinOffset() const;
  virtual int CaretMaxOffset() const;
  unsigned ResolvedTextLength() const;

  // True if any character remains after CSS white-space collapsing.
  bool HasNonCollapsedText() const;

  bool ContainsReversedText() const {
    NOT_DESTROYED();
    return contains_reversed_text_;
  }

  bool IsSecure() const {
    NOT_DESTROYED();
    return StyleRef().TextSecurity() != ETextSecurity::kNone;
  }
  void MomentarilyRevealLastTypedCharacter(
      unsigned last_typed_character_offset);

  bool IsAllCollapsibleWhitespace() const;

  void RemoveAndDestroyTextBoxes();

  AbstractInlineTextBox* FirstAbstractInlineTextBox();

  bool HasAbstractInlineTextBox() const {
    NOT_DESTROYED();
    return has_abstract_inline_text_box_;
  }

  void SetHasAbstractInlineTextBox() {
    NOT_DESTROYED();
    has_abstract_inline_text_box_ = true;
  }

  PhysicalRect DebugRect() const override;

  void AutosizingMultiplerChanged() {
    NOT_DESTROYED();
    known_to_have_no_overflow_and_no_fallback_fonts_ = false;

    // The font size is changing, so we need to make sure to rebuild everything.
    valid_ng_items_ = false;
    SetNeedsCollectInlines();
  }

  OnlyWhitespaceOrNbsp ContainsOnlyWhitespaceOrNbsp() const;

  virtual UChar PreviousCharacter() const;

  // Returns the OffsetMapping object when the current text is laid out with
  // LayoutNG.
  // Note that the text can be in legacy layout even when LayoutNG is enabled,
  // so we can't simply check the RuntimeEnabledFeature.
  const OffsetMapping* GetOffsetMapping() const;

  // Map DOM offset to LayoutNG text content offset.
  // Returns false if all characters in this LayoutText are collapsed.
  bool MapDOMOffsetToTextContentOffset(const OffsetMapping&,
                                       unsigned* start,
                                       unsigned* end) const;
  DOMNodeId EnsureNodeId();
  bool HasNodeId() const {
    NOT_DESTROYED();
    return node_id_ != kInvalidDOMNodeId;
  }

  void SetInlineItems(InlineItemsData* data, wtf_size_t begin, wtf_size_t size);
  void ClearInlineItems();
  bool HasValidInlineItems() const {
    NOT_DESTROYED();
    return valid_ng_items_;
  }
  const InlineItemSpan& InlineItems() const;
  // Inline items depends on context. It needs to be invalidated not only when
  // it was inserted/changed but also it was moved.
  void InvalidateInlineItems() {
    NOT_DESTROYED();
    valid_ng_items_ = false;
  }

  bool HasBidiControlInlineItems() const {
    NOT_DESTROYED();
    return has_bidi_control_items_;
  }
  void SetHasBidiControlInlineItems() {
    NOT_DESTROYED();
    has_bidi_control_items_ = true;
  }
  void ClearHasBidiControlInlineItems() {
    NOT_DESTROYED();
    has_bidi_control_items_ = false;
  }

  const InlineItemSpan* GetInlineItems() const {
    NOT_DESTROYED();
    return &inline_items_;
  }
  InlineItemSpan* GetInlineItems() {
    NOT_DESTROYED();
    return &inline_items_;
  }

  void InvalidateSubtreeLayoutForFontUpdates() override;

  void DetachAbstractInlineTextBoxesIfNeeded();

  // Returns the logical location of the first line box, and the logical height
  // of the LayoutText.
  void LogicalStartingPointAndHeight(LogicalOffset& logical_starting_point,
                                     LayoutUnit& logical_height) const;

  // For LayoutShiftTracker. Saves the value of LogicalStartingPoint() value
  // during the previous paint invalidation.
  LogicalOffset PreviousLogicalStartingPoint() const {
    NOT_DESTROYED();
    return previous_logical_starting_point_;
  }
  // This is const because LayoutObjects are const for paint invalidation.
  void SetPreviousLogicalStartingPoint(const LogicalOffset& point) const {
    NOT_DESTROYED();
    DCHECK_EQ(GetDocument().Lifecycle().GetState(),
              DocumentLifecycle::kInPrePaint);
    previous_logical_starting_point_ = point;
  }
  static LogicalOffset UninitializedLogicalStartingPoint() {
    return {LayoutUnit::Max(), LayoutUnit::Max()};
  }

#if DCHECK_IS_ON()
  void RecalcVisualOverflow() override;
#endif

 protected:
  void WillBeDestroyed() override;

  void StyleWillChange(StyleDifference, const ComputedStyle&) final;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void InLayoutNGInlineFormattingContextWillChange(bool) final;

  virtual void TextDidChange();

  void InvalidatePaint(const PaintInvalidatorContext&) const final;
  void InvalidateDisplayItemClients(PaintInvalidationReason) const final;

  bool CanBeSelectionLeafInternal() const final {
    NOT_DESTROYED();
    return true;
  }

  // Override |LayoutObject| implementation to invalidate |LayoutNGtextCombine|.
  // Note: This isn't a virtual function.
  void SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReasonForTracing reason);

 private:
  void TextDidChangeWithoutInvalidation();

  // PhysicalRectCollector should be like a function:
  // void (const PhysicalRect&).
  template <typename PhysicalRectCollector>
  void CollectLineBoxRects(const PhysicalRectCollector&,
                           ClippingOption option = kNoClipping) const;

  // See the class comment as to why we shouldn't call this function directly.
  void Paint(const PaintInfo&) const final {
    NOT_DESTROYED();
    NOTREACHED();
  }
  void UpdateLayout() final {
    NOT_DESTROYED();
    NOTREACHED();
  }
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset&,
                   HitTestPhase) final {
    NOT_DESTROYED();
    NOTREACHED();
    return false;
  }

  void DeleteTextBoxes();

  void ApplyTextTransform();
  void SecureText(UChar mask);

  // This will catch anyone doing an unnecessary check.
  bool IsText() const = delete;

  PhysicalRect LocalVisualRectIgnoringVisibility() const final;

  const DisplayItemClient* GetSelectionDisplayItemClient() const final;

  // We put the bitfield first to minimize padding on 64-bit.
 protected:
  // Whether or not we can be broken into multiple lines.
  unsigned has_breakable_char_ : 1;
  // Whether or not we have a hard break (e.g., <pre> with '\n').
  unsigned has_break_ : 1;
  // Whether or not we have a variable width tab character (e.g., <pre> with
  // '\t').
  unsigned has_tab_ : 1;
  unsigned has_breakable_start_ : 1;
  unsigned has_breakable_end_ : 1;
  unsigned has_end_white_space_ : 1;
  // This bit indicates that the text run has already dirtied specific line
  // boxes, and this hint will enable layoutInlineChildren to avoid just
  // dirtying everything when character data is modified (e.g., appended/
  // inserted or removed).
  unsigned lines_dirty_ : 1;

  // Whether the InlineItems associated with this object are valid. Set after
  // layout and cleared whenever the LayoutText is modified.
  // Functionally the inverse equivalent of lines_dirty_ for LayoutNG.
  unsigned valid_ng_items_ : 1;

  // Whether there is any BidiControl type InlineItem associated with this
  // object. Set after layout when associating items.
  unsigned has_bidi_control_items_ : 1;

  unsigned contains_reversed_text_ : 1;
  mutable unsigned known_to_have_no_overflow_and_no_fallback_fonts_ : 1;
  unsigned contains_only_whitespace_or_nbsp_ : 2;

  unsigned is_text_fragment_ : 1;

 private:
  ContentCaptureManager* GetOrResetContentCaptureManager();
  void DetachAbstractInlineTextBoxes();

  // Used for LayoutNG with accessibility. True if inline fragments are
  // associated to |AbstractInlineTextBox|.
  unsigned has_abstract_inline_text_box_ : 1;

  DOMNodeId node_id_ = kInvalidDOMNodeId;

  float min_width_;
  float max_width_;
  float first_line_min_width_;
  float last_line_line_min_width_;

  String text_;

  // This is mutable for paint invalidation.
  mutable LogicalOffset previous_logical_starting_point_ =
      UninitializedLogicalStartingPoint();

  InlineItemSpan inline_items_;

  // The index of the first fragment item associated with this object in
  // |FragmentItems::Items()|. Zero means there are no such item.
  // Valid only when IsInLayoutNGInlineFormattingContext().
  wtf_size_t first_fragment_item_index_ = 0u;
};

inline wtf_size_t LayoutText::FirstInlineFragmentItemIndex() const {
  if (!IsInLayoutNGInlineFormattingContext())
    return 0u;
  return first_fragment_item_index_;
}

inline UChar LayoutText::UncheckedCharacterAt(unsigned i) const {
  SECURITY_DCHECK(i < TextLength());
  return Is8Bit() ? Characters8()[i] : Characters16()[i];
}

inline UChar LayoutText::CharacterAt(unsigned i) const {
  if (i >= TextLength())
    return 0;

  return UncheckedCharacterAt(i);
}

inline UChar32 LayoutText::CodepointAt(unsigned i) const {
  if (i >= TextLength())
    return 0;
  if (Is8Bit())
    return Characters8()[i];
  UChar32 c;
  U16_GET(Characters16(), 0, i, TextLength(), c);
  return c;
}

inline void LayoutText::DetachAbstractInlineTextBoxesIfNeeded() {
  if (UNLIKELY(has_abstract_inline_text_box_))
    DetachAbstractInlineTextBoxes();
}

template <>
struct DowncastTraits<LayoutText> {
  static bool AllowFrom(const LayoutObject& object) { return object.IsText(); }
};

inline LayoutText* Text::GetLayoutObject() const {
  return To<LayoutText>(CharacterData::GetLayoutObject());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_H_
