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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_RUN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_RUN_H_

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

namespace blink {

class LayoutRubyBase;
class LayoutRubyText;
template <typename Base>
class LayoutNGMixin;

// LayoutRubyRun are 'inline-block/table' like objects,and wrap a single pairing
// of a ruby base with its ruby text(s).
// See LayoutRuby.h for further comments on the structure

class LayoutRubyRun : public LayoutBlockFlow {
 public:
  ~LayoutRubyRun() override;

  bool HasRubyText() const;
  bool HasRubyBase() const;
  LayoutRubyText* RubyText() const;
  LayoutRubyBase* RubyBase() const;
  LayoutRubyBase*
  RubyBaseSafe();  // creates the base if it doesn't already exist

  LayoutObject* LayoutSpecialExcludedChild(bool relayout_children,
                                           SubtreeLayoutScope&) override;
  void UpdateLayout() override;

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;
  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject* child) override;

  void GetOverhang(bool first_line,
                   LayoutObject* start_layout_object,
                   LayoutObject* end_layout_object,
                   int& start_overhang,
                   int& end_overhang) const;

  static LayoutRubyRun* StaticCreateRubyRun(
      const LayoutObject* parent_ruby,
      const LayoutBlock& containing_block);

  bool CanBreakBefore(const LazyLineBreakIterator&) const;

  const char* GetName() const override { return "LayoutRubyRun"; }

 protected:
  LayoutRubyBase* CreateRubyBase() const;

 private:
  // The argument must be nullptr.
  explicit LayoutRubyRun(Element*);

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectRubyRun || LayoutBlockFlow::IsOfType(type);
  }
  bool CreatesAnonymousWrapper() const override { return true; }
  void RemoveLeftoverAnonymousBlock(LayoutBlock*) override {}

  friend class LayoutNGMixin<LayoutRubyRun>;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutRubyRun, IsRubyRun());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_RUN_H_
