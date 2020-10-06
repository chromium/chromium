// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BREAK_TOKEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BREAK_TOKEN_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_appeal.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

// A break token is a continuation token for layout. A single layout input node
// can have multiple fragments asssociated with it.
//
// Each fragment has a break token which can be used to determine if a layout
// input node has finished producing fragments (aka. is "exhausted" of
// fragments), and optionally used to produce the next fragment in the chain.
//
// See CSS Fragmentation (https://drafts.csswg.org/css-break/) for a detailed
// description of different types of breaks which can occur in CSS.
//
// Each layout algorithm which can fragment, e.g. block-flow can optionally
// accept a break token. For example:
//
// NGLayoutInputNode* node = ...;
// NGPhysicalFragment* fragment = node->Layout(space);
// DCHECK(!fragment->BreakToken()->IsFinished());
// NGPhysicalFragment* fragment2 = node->Layout(space, fragment->BreakToken());
//
// The break token should encapsulate enough information to "resume" the layout.
class CORE_EXPORT NGBreakToken : public RefCounted<NGBreakToken> {
  USING_FAST_MALLOC(NGBreakToken);

 public:
  virtual ~NGBreakToken() = default;

  enum NGBreakTokenType {
    kBlockBreakToken = NGLayoutInputNode::kBlock,
    kInlineBreakToken = NGLayoutInputNode::kInline
  };
  NGBreakTokenType Type() const { return static_cast<NGBreakTokenType>(type_); }

  bool IsBlockType() const { return Type() == kBlockBreakToken; }
  bool IsInlineType() const { return Type() == kInlineBreakToken; }

  enum NGBreakTokenStatus { kUnfinished, kFinished };

  // Whether the layout node cannot produce any more fragments.
  bool IsFinished() const { return status_ == kFinished; }

  // Returns the node associated with this break token. A break token cannot be
  // used with any other node.
  NGLayoutInputNode InputNode() const {
    return NGLayoutInputNode::Create(
        box_, static_cast<NGLayoutInputNode::NGLayoutInputNodeType>(type_));
  }

  NGBreakAppeal BreakAppeal() const {
    return static_cast<NGBreakAppeal>(break_appeal_);
  }

#if DCHECK_IS_ON()
  virtual String ToString() const;
  void ShowBreakTokenTree() const;
#endif

 protected:
  NGBreakToken(NGBreakTokenType type,
               NGBreakTokenStatus status,
               NGLayoutInputNode node)
      : box_(node.GetLayoutBox()),
        type_(type),
        status_(status),
        flags_(0),
        is_break_before_(false),
        is_forced_break_(false),
        is_at_block_end_(false),
        break_appeal_(kBreakAppealPerfect),
        has_seen_all_children_(false) {
    DCHECK_EQ(type, static_cast<NGBreakTokenType>(node.Type()));
  }

 private:
  // Because |NGLayoutInputNode| has a pointer and 1 bit flag, and it's fast to
  // re-construct, keep |LayoutBox| to save the memory consumed by alignment.
  LayoutBox* box_;

  unsigned type_ : 1;
  unsigned status_ : 1;

 protected:
  // The following bitfields are only to be used by NGInlineBreakToken (it's
  // defined here to save memory, since that class has no bitfields).

  unsigned flags_ : 2;  // NGInlineBreakTokenFlags

  // The following bitfields are only to be used by NGBlockBreakToken (it's
  // defined here to save memory, since that class has no bitfields).

  unsigned is_break_before_ : 1;

  unsigned is_forced_break_ : 1;

  // Set when layout is past the block-end border edge. If we break when we're
  // in this state, it means that something is overflowing, and thus establishes
  // a parallel flow.
  unsigned is_at_block_end_ : 1;

  // If the break is unforced, this is the appeal of the break. Higher is
  // better. Violating breaking rules decreases appeal. Forced breaks always
  // have perfect appeal.
  unsigned break_appeal_ : 2;  // NGBreakAppeal

  // All children of this container have been "seen" at this point. This means
  // that all children have been fully laid out, or have break tokens. No more
  // children left to discover.
  unsigned has_seen_all_children_ : 1;
};

typedef Vector<scoped_refptr<const NGBreakToken>> NGBreakTokenVector;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BREAK_TOKEN_H_
