// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

// Represents a ruby base box.
// https://drafts.csswg.org/css-ruby-1/#ruby-base-box.
class CORE_EXPORT LayoutRubyBase final : public LayoutBlockFlow {
 public:
  explicit LayoutRubyBase();
  ~LayoutRubyBase() override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutRubyBase";
  }
  bool IsRubyBase() const final {
    NOT_DESTROYED();
    return true;
  }
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  // This function removes all children that are before (!) `before_child`
  // and appends them to `to_base`.
  void MoveChildren(LayoutRubyBase& to_base,
                    LayoutObject* before_child = nullptr);

  // Returns true if this object was created for a RubyText without a
  // corresponding RubyBase.
  bool IsPlaceholder() const;
  void SetPlaceholder();

 private:
  void MoveInlineChildrenTo(LayoutRubyBase& to_base,
                            LayoutObject* before_child);
  void MoveBlockChildrenTo(LayoutRubyBase& to_base, LayoutObject* before_child);

  bool is_placeholder_ = false;
};

template <>
struct DowncastTraits<LayoutRubyBase> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsRubyBase();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_BASE_H_
