// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_AS_BLOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_AS_BLOCK_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_ng_block_flow.h"

namespace blink {

class RubyContainer;

// Represents <ruby> with "display: block" or "display: inline-block".
// If we supports "display: block ruby", we can remove this class.
class CORE_EXPORT LayoutRubyAsBlock : public LayoutNGBlockFlow {
 public:
  explicit LayoutRubyAsBlock(Element*);
  ~LayoutRubyAsBlock() override;
  void Trace(Visitor* visitor) const override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutRubyAsBlock";
  }
  bool IsOfType(LayoutObjectType type) const override;
  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject* child) override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void RemoveLeftoverAnonymousBlock(LayoutBlock*) override;

  void DidRemoveChildFromColumn(LayoutObject& child);

 private:
  Member<RubyContainer> ruby_container_;
};

template <>
struct DowncastTraits<LayoutRubyAsBlock> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsRuby() && !object.IsLayoutInline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_AS_BLOCK_H_
