// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/line/abstract_inline_text_box.h"

namespace blink {

class NGFragmentItem;
class NGInlineCursor;

// The implementation of |AbstractInlineTextBox| for LayoutNG.
// See also |LegacyAbstractInlineTextBox| for legacy layout.
class CORE_EXPORT NGAbstractInlineTextBox final : public AbstractInlineTextBox {
 private:
  // Returns existing or newly created |NGAbstractInlineTextBox|.
  // * |cursor| should be attached to a text item.
  static scoped_refptr<AbstractInlineTextBox> GetOrCreate(
      const NGInlineCursor& cursor);
  static void WillDestroy(const NGInlineCursor& cursor);

  friend class LayoutText;

 public:
  explicit NGAbstractInlineTextBox(const NGInlineCursor& cursor);
  ~NGAbstractInlineTextBox() final;

 private:
  NGInlineCursor GetCursor() const;
  NGInlineCursor GetCursorOnLine() const;
  String GetTextContent() const;

  // Implementations of AbstractInlineTextBox member functions.
  void Detach() final;
  scoped_refptr<AbstractInlineTextBox> NextInlineTextBox() const final;
  LayoutRect LocalBounds() const final;
  unsigned Len() const final;
  unsigned TextOffsetInFormattingContext(unsigned offset) const final;
  Direction GetDirection() const final;
  void CharacterWidths(Vector<float>&) const final;
  String GetText() const final;
  bool IsFirst() const final;
  bool IsLast() const final;
  scoped_refptr<AbstractInlineTextBox> NextOnLine() const final;
  scoped_refptr<AbstractInlineTextBox> PreviousOnLine() const final;
  bool IsLineBreak() const final;
  bool NeedsTrailingSpace() const final;

  const NGFragmentItem* fragment_item_;
  // |root_box_fragment_| owns |fragment_item_|. Persistent is used here to keep
  // |NGAbstractInlineTextBoxCache| off-heap.
  Persistent<const NGPhysicalBoxFragment> root_box_fragment_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_
