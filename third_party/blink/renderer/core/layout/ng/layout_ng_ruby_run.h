// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_RUN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_RUN_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

class LayoutNGRubyBase;
class LayoutNGRubyText;

// LayoutNGRubyRun are 'inline-block/table' like objects,and wrap a single
// pairing of a ruby base with its ruby text(s).
// See layout_ruby.h for further comments on the structure
class CORE_EXPORT LayoutNGRubyRun final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGRubyRun();
  ~LayoutNGRubyRun() override;
  static LayoutNGRubyRun& Create(const LayoutObject* parent_ruby,
                                 const LayoutBlock& containing_block);

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGRubyRun";
  }

  bool HasRubyText() const;
  bool HasRubyBase() const;
  LayoutNGRubyText* RubyText() const;
  LayoutNGRubyBase* RubyBase() const;
  // Creates the base if it doesn't already exist
  LayoutNGRubyBase& EnsureRubyBase();

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;
  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject* child) override;
  bool IsOfType(LayoutObjectType type) const override;
  bool CreatesAnonymousWrapper() const override;
  void RemoveLeftoverAnonymousBlock(LayoutBlock*) override;

 private:
  LayoutNGRubyBase& CreateRubyBase() const;
};

template <>
struct DowncastTraits<LayoutNGRubyRun> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsRubyRun();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_RUN_H_
