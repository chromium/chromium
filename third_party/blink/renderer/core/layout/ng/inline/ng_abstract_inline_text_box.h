// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class NGFragmentItem;
class NGInlineCursor;

// High-level abstraction of a text box fragment, to allow the accessibility
// module to get information without tight coupling.
class CORE_EXPORT NGAbstractInlineTextBox final
    : public RefCounted<NGAbstractInlineTextBox> {
 private:
  // Returns existing or newly created |NGAbstractInlineTextBox|.
  // * |cursor| should be attached to a text item.
  static scoped_refptr<NGAbstractInlineTextBox> GetOrCreate(
      const NGInlineCursor& cursor);
  static void WillDestroy(const NGInlineCursor& cursor);

  friend class LayoutText;

 public:
  explicit NGAbstractInlineTextBox(const NGInlineCursor& cursor);
  ~NGAbstractInlineTextBox();

  struct WordBoundaries {
    DISALLOW_NEW();
    WordBoundaries(int start_index, int end_index)
        : start_index(start_index), end_index(end_index) {}
    int start_index;
    int end_index;
  };
  static void GetWordBoundariesForText(Vector<WordBoundaries>&, const String&);

  void Detach();
  scoped_refptr<NGAbstractInlineTextBox> NextInlineTextBox() const;
  LayoutRect LocalBounds() const;
  unsigned Len() const;
  // Given a text offset in this inline text box, returns the equivalent text
  // offset in this box's formatting context. The formatting context is the
  // deepest block flow ancestor, e.g. the enclosing paragraph. A "text offset",
  // in contrast to a "DOM offset", is an offset in the box's text after any
  // collapsible white space in the DOM has been collapsed.
  unsigned TextOffsetInFormattingContext(unsigned offset) const;
  enum Direction { kLeftToRight, kRightToLeft, kTopToBottom, kBottomToTop };
  Direction GetDirection() const;
  Node* GetNode() const;
  LayoutText* GetLayoutText() const { return layout_text_; }
  AXObjectCache* ExistingAXObjectCache() const;
  void CharacterWidths(Vector<float>&) const;
  void GetWordBoundaries(Vector<WordBoundaries>&) const;
  String GetText() const;
  bool IsFirst() const;
  bool IsLast() const;
  scoped_refptr<NGAbstractInlineTextBox> NextOnLine() const;
  scoped_refptr<NGAbstractInlineTextBox> PreviousOnLine() const;
  bool IsLineBreak() const;
  bool NeedsTrailingSpace() const;

 private:
  LayoutText* GetFirstLetterPseudoLayoutText() const;
  NGInlineCursor GetCursor() const;
  NGInlineCursor GetCursorOnLine() const;
  String GetTextContent() const;

  WeakPersistent<LayoutText> layout_text_;
  const NGFragmentItem* fragment_item_;
  // |root_box_fragment_| owns |fragment_item_|. Persistent is used here to keep
  // |NGAbstractInlineTextBoxCache| off-heap.
  Persistent<const NGPhysicalBoxFragment> root_box_fragment_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_
