// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_BREAK_TOKEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_BREAK_TOKEN_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// Represents a break token for an inline node.
class CORE_EXPORT NGInlineBreakToken final : public NGBreakToken {
 public:
  enum NGInlineBreakTokenFlags {
    kDefault = 0,
    kIsForcedBreak = 1 << 0,
    kUseFirstLineStyle = 1 << 1
    // When adding values, ensure |flags_| has enough storage.
  };

  // Creates a break token for a node which did fragment, and can potentially
  // produce more fragments.
  // Takes ownership of the state_stack.
  static scoped_refptr<NGInlineBreakToken> Create(
      NGInlineNode node,
      const ComputedStyle* style,
      unsigned item_index,
      unsigned text_offset,
      unsigned flags /* NGInlineBreakTokenFlags */) {
    return base::AdoptRef(
        new NGInlineBreakToken(node, style, item_index, text_offset, flags));
  }

  // Creates a break token for a node which cannot produce any more fragments.
  static scoped_refptr<NGInlineBreakToken> Create(NGLayoutInputNode node) {
    return base::AdoptRef(new NGInlineBreakToken(node));
  }

  ~NGInlineBreakToken() override;

  // The style at the end of this break token. The next line should start with
  // this style.
  const ComputedStyle* Style() const {
    DCHECK(!IsFinished());
    return style_.get();
  }

  unsigned ItemIndex() const {
    DCHECK(!IsFinished());
    return item_index_;
  }

  unsigned TextOffset() const {
    DCHECK(!IsFinished());
    return text_offset_;
  }

  bool UseFirstLineStyle() const {
    DCHECK(!IsFinished());
    return flags_ & kUseFirstLineStyle;
  }

  bool IsForcedBreak() const {
    DCHECK(!IsFinished());
    return flags_ & kIsForcedBreak;
  }

#if DCHECK_IS_ON()
  String ToString() const override;
#endif

 private:
  NGInlineBreakToken(NGInlineNode node,
                     const ComputedStyle*,
                     unsigned item_index,
                     unsigned text_offset,
                     unsigned flags /* NGInlineBreakTokenFlags */);

  explicit NGInlineBreakToken(NGLayoutInputNode node);

  scoped_refptr<const ComputedStyle> style_;
  unsigned item_index_;
  unsigned text_offset_;
};

template <>
struct DowncastTraits<NGInlineBreakToken> {
  static bool AllowFrom(const NGBreakToken& token) {
    return token.IsInlineType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_BREAK_TOKEN_H_
