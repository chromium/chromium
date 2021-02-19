// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_EARLY_BREAK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_EARLY_BREAK_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

namespace blink {

// Possible early unforced breakpoint. This represents a possible (and good)
// location to break. In cases where we run out of space at an unideal location,
// we may want to go back and break here instead.
class NGEarlyBreak : public RefCounted<NGEarlyBreak> {
 public:
  enum BreakType {
    kLine,  // Break before a specified line number.
    kBlock  // Break before or inside a specified child block.
  };

  explicit NGEarlyBreak(
      NGBlockNode block,
      scoped_refptr<const NGEarlyBreak> break_inside_child = nullptr)
      : box_(block.GetLayoutBox()),
        break_inside_child_(break_inside_child),
        type_(kBlock) {}
  explicit NGEarlyBreak(int line_number)
      : line_number_(line_number), type_(kLine) {}

  BreakType Type() const { return type_; }
  bool IsBreakBefore() const { return !break_inside_child_; }
  NGBlockNode BlockNode() const {
    CHECK_EQ(type_, kBlock);
    return NGBlockNode(box_);
  }
  int LineNumber() const {
    DCHECK_EQ(type_, kLine);
    return line_number_;
  }
  const NGEarlyBreak* BreakInside() const { return break_inside_child_.get(); }

 private:
  union {
    LayoutBox* box_;   // Set if type_ == kBlock
    int line_number_;  // Set if type_ == kLine
  };
  scoped_refptr<const NGEarlyBreak> break_inside_child_;
  BreakType type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_EARLY_BREAK_H_
