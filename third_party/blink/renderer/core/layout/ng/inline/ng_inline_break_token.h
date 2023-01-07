// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_BREAK_TOKEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_BREAK_TOKEN_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class NGBlockBreakToken;

// Represents a break token for an inline node.
class CORE_EXPORT NGInlineBreakToken final : public NGBreakToken {
 public:
  enum NGInlineBreakTokenFlags {
    kDefault = 0,
    kIsForcedBreak = 1 << 0,
    kHasSubBreakToken = 1 << 1,
    kUseFirstLineStyle = 1 << 2,
    kHasClonedBoxDecorations = 1 << 3,
    // When adding values, ensure |flags_| has enough storage.
  };

  // Creates a break token for a node which did fragment, and can potentially
  // produce more fragments.
  // Takes ownership of the state_stack.
  static NGInlineBreakToken* Create(
      NGInlineNode node,
      const ComputedStyle* style,
      unsigned item_index,
      unsigned text_offset,
      unsigned flags /* NGInlineBreakTokenFlags */,
      const NGBreakToken* sub_break_token = nullptr);

  // The style at the end of this break token. The next line should start with
  // this style.
  const ComputedStyle* Style() const { return style_.get(); }

  unsigned ItemIndex() const {
    return item_index_;
  }

  unsigned TextOffset() const {
    return text_offset_;
  }

  bool UseFirstLineStyle() const {
    return flags_ & kUseFirstLineStyle;
  }

  bool IsForcedBreak() const {
    return flags_ & kIsForcedBreak;
  }

  // True if this is after a block-in-inline.
  bool IsAfterBlockInInline() const;

  // The BreakToken when a block-in-inline is block-fragmented.
  const NGBlockBreakToken* BlockInInlineBreakToken() const;

  // The BreakToken for the inline break token that has a block in inline break
  // token inside. This should be resumed in the next fragmentainer as a
  // parallel flow. This happens when a block inside an inline is overflowed
  // beyond what was available in the fragmentainer. Regular inline content will
  // then still fit in the fragmentainer, while the block inside the inline will
  // be resumed in the next fragmentainer.
  const NGInlineBreakToken* SubBreakTokenInParallelFlow() const;

  // True if the current position has open tags that has `box-decoration-break:
  // clone`. They should be cloned to the start of the next line.
  bool HasClonedBoxDecorations() const {
    return flags_ & kHasClonedBoxDecorations;
  }

  using PassKey = base::PassKey<NGInlineBreakToken>;
  NGInlineBreakToken(PassKey,
                     NGInlineNode node,
                     const ComputedStyle*,
                     unsigned item_index,
                     unsigned text_offset,
                     unsigned flags /* NGInlineBreakTokenFlags */,
                     const NGBreakToken* sub_break_token);

  explicit NGInlineBreakToken(PassKey, NGLayoutInputNode node);

#if DCHECK_IS_ON()
  String ToString() const;
#endif

  void TraceAfterDispatch(Visitor*) const;

 private:
  const Member<const NGBreakToken>* SubBreakTokenAddress() const;

  scoped_refptr<const ComputedStyle> style_;
  unsigned item_index_;
  unsigned text_offset_;

  // This is an array of one item if |kHasSubBreakToken|, or zero.
  Member<const NGBreakToken> sub_break_token_[];
};

template <>
struct DowncastTraits<NGInlineBreakToken> {
  static bool AllowFrom(const NGBreakToken& token) {
    return token.IsInlineType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_BREAK_TOKEN_H_
