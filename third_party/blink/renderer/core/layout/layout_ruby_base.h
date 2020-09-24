/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_BASE_H_

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

class LayoutRubyRun;
template <typename Base>
class LayoutNGMixin;

class LayoutRubyBase : public LayoutBlockFlow {
 public:
  ~LayoutRubyBase() override;
  static LayoutRubyBase* CreateAnonymous(Document*,
                                         const LayoutRubyRun& ruby_run);

  const char* GetName() const override { return "LayoutRubyBase"; }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectRubyBase || LayoutBlockFlow::IsOfType(type);
  }

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

 private:
  // The argument must be nullptr. It's necessary for the LayoutNGMixin
  // constructor.
  explicit LayoutRubyBase(Element*);

  ETextAlign TextAlignmentForLine(bool ends_with_soft_break) const override;
  void AdjustInlineDirectionLineBounds(
      unsigned expansion_opportunity_count,
      LayoutUnit& logical_left,
      LayoutUnit& logical_width) const override;

  void MoveChildren(LayoutRubyBase* to_base,
                    LayoutObject* before_child = nullptr);
  void MoveInlineChildren(LayoutRubyBase* to_base,
                          LayoutObject* before_child = nullptr);
  void MoveBlockChildren(LayoutRubyBase* to_base,
                         LayoutObject* before_child = nullptr);

  friend class LayoutNGMixin<LayoutRubyBase>;
  // Allow LayoutRubyRun to manipulate the children within ruby bases.
  friend class LayoutRubyRun;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutRubyBase, IsRubyBase());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_BASE_H_
