// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_

#include "third_party/blink/renderer/core/layout/line/abstract_inline_text_box.h"

namespace blink {

class NGPaintFragment;
class NGPhysicalTextFragment;

// The implementation of |AbstractInlineTextBox| for LayoutNG.
// See also |LegacyAbstractInlineTextBox| for legacy layout.
class CORE_EXPORT NGAbstractInlineTextBox final : public AbstractInlineTextBox {
 private:
  // Returns existing or newly created |NGAbstractInlineTextBox|.
  // * |fragment| should be attached to |NGPhysicalTextFragment|.
  static scoped_refptr<AbstractInlineTextBox> GetOrCreate(
      const NGPaintFragment& fragment);
  static void WillDestroy(NGPaintFragment*);

  friend class LayoutText;
  friend class NGPaintFragment;

 public:
  ~NGAbstractInlineTextBox() final;

 private:
  NGAbstractInlineTextBox(LineLayoutText line_layout_item,
                          const NGPaintFragment& fragment);

  const NGPhysicalTextFragment& PhysicalTextFragment() const;
  bool NeedsLayout() const;
  bool NeedsTrailingSpace() const;
  // Returns next fragment associated to |LayoutText|.
  const NGPaintFragment* NextTextFragmentForSameLayoutObject() const;

  // Implementations of AbstractInlineTextBox member functions.
  void Detach() final;
  scoped_refptr<AbstractInlineTextBox> NextInlineTextBox() const final;
  LayoutRect LocalBounds() const final;
  unsigned Len() const final;
  unsigned TextOffsetInContainer(unsigned offset) const final;
  Direction GetDirection() const final;
  void CharacterWidths(Vector<float>&) const final;
  String GetText() const final;
  bool IsFirst() const final;
  bool IsLast() const final;
  scoped_refptr<AbstractInlineTextBox> NextOnLine() const final;
  scoped_refptr<AbstractInlineTextBox> PreviousOnLine() const final;
  bool IsLineBreak() const final;

  const NGPaintFragment* fragment_;

  using FragmentToNGAbstractInlineTextBoxHashMap =
      HashMap<const NGPaintFragment*, scoped_refptr<AbstractInlineTextBox>>;
  static FragmentToNGAbstractInlineTextBoxHashMap*
      g_abstract_inline_text_box_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_ABSTRACT_INLINE_TEXT_BOX_H_
