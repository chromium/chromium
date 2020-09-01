/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_ABSTRACT_INLINE_TEXT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_ABSTRACT_INLINE_TEXT_BOX_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_text.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class InlineTextBox;

// High-level abstraction of InlineTextBox to allow the accessibility module to
// get information about InlineTextBoxes without tight coupling.
class CORE_EXPORT AbstractInlineTextBox
    : public RefCounted<AbstractInlineTextBox> {
 public:
  struct WordBoundaries {
    DISALLOW_NEW();
    WordBoundaries(int start_index, int end_index)
        : start_index(start_index), end_index(end_index) {}
    int start_index;
    int end_index;
  };

  enum Direction { kLeftToRight, kRightToLeft, kTopToBottom, kBottomToTop };

  virtual ~AbstractInlineTextBox();

  LineLayoutText GetLineLayoutItem() const { return line_layout_item_; }

  virtual void Detach();
  virtual scoped_refptr<AbstractInlineTextBox> NextInlineTextBox() const = 0;
  virtual LayoutRect LocalBounds() const = 0;
  virtual unsigned Len() const = 0;
  // Given a text offset in this inline text box, returns the equivalent text
  // offset in this box's formatting context. The formatting context is the
  // deepest block flow ancestor, e.g. the enclosing paragraph. A "text offset",
  // in contrast to a "DOM offset", is an offset in the box's text after any
  // collapsible white space in the DOM has been collapsed.
  virtual unsigned TextOffsetInFormattingContext(unsigned) const = 0;
  virtual Direction GetDirection() const = 0;
  Node* GetNode() const;
  virtual void CharacterWidths(Vector<float>&) const = 0;
  void GetWordBoundaries(Vector<WordBoundaries>&) const;
  virtual String GetText() const = 0;
  virtual bool IsFirst() const = 0;
  virtual bool IsLast() const = 0;
  virtual scoped_refptr<AbstractInlineTextBox> NextOnLine() const = 0;
  virtual scoped_refptr<AbstractInlineTextBox> PreviousOnLine() const = 0;
  virtual bool IsLineBreak() const = 0;
  virtual bool NeedsTrailingSpace() const = 0;

 protected:
  explicit AbstractInlineTextBox(LineLayoutText line_layout_item);

  LayoutText* GetFirstLetterPseudoLayoutText() const;

 private:
  // Weak ptrs; these are nulled when InlineTextBox::destroy() calls
  // AbstractInlineTextBox::willDestroy.
  LineLayoutText line_layout_item_;
};

// The implementation of |AbstractInlineTextBox| for legacy layout.
// See also |NGAbstractInlineTextBox| for LayoutNG.
class CORE_EXPORT LegacyAbstractInlineTextBox final
    : public AbstractInlineTextBox {
 private:
  LegacyAbstractInlineTextBox(LineLayoutText line_layout_item,
                              InlineTextBox* inline_text_box);

  static scoped_refptr<AbstractInlineTextBox> GetOrCreate(LineLayoutText,
                                                          InlineTextBox*);
  static void WillDestroy(InlineTextBox*);

  friend class LayoutText;
  friend class InlineTextBox;

 public:
  ~LegacyAbstractInlineTextBox() final;

 private:
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

  InlineTextBox* inline_text_box_;

  typedef HashMap<InlineTextBox*, scoped_refptr<AbstractInlineTextBox>>
      InlineToLegacyAbstractInlineTextBoxHashMap;
  static InlineToLegacyAbstractInlineTextBoxHashMap*
      g_abstract_inline_text_box_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_ABSTRACT_INLINE_TEXT_BOX_H_
