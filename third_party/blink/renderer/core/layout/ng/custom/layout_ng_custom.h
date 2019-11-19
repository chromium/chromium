// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_LAYOUT_NG_CUSTOM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_LAYOUT_NG_CUSTOM_H_

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// NOTE: In the future there may be a third state "normal", this will mean that
// not everything is blockified, (e.g. root inline boxes, so that line-by-line
// layout can be performed).
enum LayoutNGCustomState { kUnloaded, kBlock };

// The LayoutObject for elements which have "display: layout(foo);" specified.
// https://drafts.css-houdini.org/css-layout-api/
//
// This class inherits from LayoutNGBlockFlow so that when a web developer's
// intrinsicSizes/layout callback fails, it can fallback onto the "default"
// block-flow layout algorithm.
class LayoutNGCustom final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGCustom(Element*);

  const char* GetName() const override { return "LayoutNGCustom"; }
  bool CreatesNewFormattingContext() const override { return true; }

  bool IsLoaded() { return state_ != kUnloaded; }

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutNGCustom ||
           LayoutNGBlockFlow::IsOfType(type);
  }

  LayoutNGCustomState state_;
};

template <>
struct DowncastTraits<LayoutNGCustom> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutNGCustom();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_LAYOUT_NG_CUSTOM_H_
