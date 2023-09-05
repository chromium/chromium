// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_COLUMN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_COLUMN_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

class LayoutNGRubyBase;
class LayoutNGRubyText;

// LayoutRubyColumn represents 'inline-block/table' like objects, and wrap a
// single pairing of a ruby base with its ruby text(s).
// https://drafts.csswg.org/css-ruby-1/#ruby-columns
//
// See layout_ruby.h for further comments on the structure
class CORE_EXPORT LayoutRubyColumn final : public LayoutNGBlockFlow {
 public:
  explicit LayoutRubyColumn();
  ~LayoutRubyColumn() override;
  static LayoutRubyColumn& Create(const LayoutObject* parent_ruby,
                                  const LayoutBlock& containing_block);

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutRubyColumn";
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
  void UpdateAnonymousChildStyle(const LayoutObject* child,
                                 ComputedStyleBuilder& builder) const override;

 private:
  LayoutNGRubyBase& CreateRubyBase() const;
};

template <>
struct DowncastTraits<LayoutRubyColumn> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsRubyColumn();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_COLUMN_H_
