// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_BREAK_TOKEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_BREAK_TOKEN_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_text_index.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class NGBlockBreakToken;

// Represents a break token for an inline node.
class CORE_EXPORT InlineBreakToken final : public NGBreakToken {
 public:
  enum InlineBreakTokenFlags {
    kDefault = 0,
    kIsForcedBreak = 1 << 0,
    kHasSubBreakToken = 1 << 1,
    kUseFirstLineStyle = 1 << 2,
    kHasClonedBoxDecorations = 1 << 3,
    kIsInParallelBlockFlow = 1 << 4,
    // When adding values, ensure |flags_| has enough storage.
  };

  // Creates a break token for a node which did fragment, and can potentially
  // produce more fragments.
  // Takes ownership of the state_stack.
  static InlineBreakToken* Create(
      InlineNode node,
      const ComputedStyle* style,
      const InlineItemTextIndex& start,
      unsigned flags /* InlineBreakTokenFlags */,
      const NGBlockBreakToken* sub_break_token = nullptr);

  // Wrap a block break token inside an inline break token. The block break
  // token may for instance be for a float inside an inline formatting context.
  // Wrapping it inside an inline break token makes it possible to resume and
  // place it correctly inside any inline ancestors.
  static InlineBreakToken* CreateForParallelBlockFlow(
      InlineNode node,
      const InlineItemTextIndex& start,
      const NGBlockBreakToken& child_break_token);

  // The style at the end of this break token. The next line should start with
  // this style.
  const ComputedStyle* Style() const { return style_.Get(); }

  // The point where the next layout should start, or where the previous layout
  // ended.
  const InlineItemTextIndex& Start() const { return start_; }
  wtf_size_t StartItemIndex() const { return start_.item_index; }
  wtf_size_t StartTextOffset() const { return start_.text_offset; }

  bool UseFirstLineStyle() const {
    return flags_ & kUseFirstLineStyle;
  }

  bool IsForcedBreak() const {
    return flags_ & kIsForcedBreak;
  }

  // The BreakToken when a block-in-inline or float is block-fragmented.
  const NGBlockBreakToken* BlockBreakToken() const;

  // True if the current position has open tags that has `box-decoration-break:
  // clone`. They should be cloned to the start of the next line.
  bool HasClonedBoxDecorations() const {
    return flags_ & kHasClonedBoxDecorations;
  }

  // True if this is to be resumed in a parallel fragmentation flow.
  // https://www.w3.org/TR/css-break-3/#parallel-flows
  bool IsInParallelBlockFlow() const { return flags_ & kIsInParallelBlockFlow; }

  using PassKey = base::PassKey<InlineBreakToken>;
  InlineBreakToken(PassKey,
                   InlineNode node,
                   const ComputedStyle*,
                   const InlineItemTextIndex& start,
                   unsigned flags /* InlineBreakTokenFlags */,
                   const NGBlockBreakToken* sub_break_token);

  explicit InlineBreakToken(PassKey, NGLayoutInputNode node);

#if DCHECK_IS_ON()
  String ToString() const;
#endif

  void TraceAfterDispatch(Visitor*) const;

 private:
  const Member<const NGBreakToken>* SubBreakTokenAddress() const;

  Member<const ComputedStyle> style_;
  InlineItemTextIndex start_;

  // This is an array of one item if |kHasSubBreakToken|, or zero.
  Member<const NGBlockBreakToken> sub_break_token_[];
};

template <>
struct DowncastTraits<InlineBreakToken> {
  static bool AllowFrom(const NGBreakToken& token) {
    return token.IsInlineType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_BREAK_TOKEN_H_
