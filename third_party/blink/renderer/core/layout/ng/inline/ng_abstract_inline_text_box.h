// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_

#include "third_party/blink/renderer/core/layout/line/abstract_inline_text_box.h"

namespace blink {

class NGFragmentItem;
class NGInlineCursor;
class NGPaintFragment;

// The implementation of |AbstractInlineTextBox| for LayoutNG.
// See also |LegacyAbstractInlineTextBox| for legacy layout.
class CORE_EXPORT NGAbstractInlineTextBox final : public AbstractInlineTextBox {
 private:
  // Returns existing or newly created |NGAbstractInlineTextBox|.
  // * |cursor| should be attached to |NGPhysicalTextFragment|.
  static scoped_refptr<AbstractInlineTextBox> GetOrCreate(
      const NGInlineCursor& cursor);
  static void WillDestroy(const NGInlineCursor& cursor);
  static void WillDestroy(const NGPaintFragment* fragment);

  friend class LayoutText;

 public:
  NGAbstractInlineTextBox(LineLayoutText line_layout_item,
                          const NGPaintFragment& fragment);
  NGAbstractInlineTextBox(LineLayoutText line_layout_item,
                          const NGFragmentItem& fragment);

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

  union {
    const NGPaintFragment* fragment_;
    const NGFragmentItem* fragment_item_;
  };
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_
