// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBreakToken_h
#define NGBreakToken_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
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
 public:
  virtual ~NGBreakToken() = default;

  enum NGBreakTokenType { kBlockBreakToken, kInlineBreakToken };
  NGBreakTokenType Type() const { return static_cast<NGBreakTokenType>(type_); }

  bool IsBlockType() const { return Type() == kBlockBreakToken; }
  bool IsInlineType() const { return Type() == kInlineBreakToken; }

  enum NGBreakTokenStatus { kUnfinished, kFinished };

  // Whether the layout node cannot produce any more fragments.
  bool IsFinished() const { return status_ == kFinished; }

  // Returns the node associated with this break token. A break token cannot be
  // used with any other node.
  NGLayoutInputNode InputNode() const { return node_; }

#ifndef NDEBUG
  virtual String ToString() const;
  void ShowBreakTokenTree() const;
#endif  // NDEBUG

 protected:
  NGBreakToken(NGBreakTokenType type,
               NGBreakTokenStatus status,
               NGLayoutInputNode node)
      : type_(type), status_(status), node_(node) {}

 private:
  unsigned type_ : 1;
  unsigned status_ : 1;

  NGLayoutInputNode node_;
};

typedef Vector<scoped_refptr<NGBreakToken>, 16> NGBreakTokenVector;

}  // namespace blink

#endif  // NGBreakToken_h
