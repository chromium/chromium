// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_COMBINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_COMBINE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// The layout object for the element having "text-combine-upright:all" in
// vertical writing mode, e.g. <i style="text-upright:all"><b>12</b>34<i>.
// Note: When the element is in horizontal writing mode, we don't use this.
// Note: Children of this class must be |LayoutText| associated to |Text| node.
class CORE_EXPORT LayoutNGTextCombine final : public LayoutNGBlockFlow {
 public:
  // Note: Mark constructor public for |MakeGarbageCollected|. We should not
  // call this directly.
  LayoutNGTextCombine();
  ~LayoutNGTextCombine() override;

  static void AssertStyleIsValid(const ComputedStyle& style);

  // Create anonymous wrapper having |text_child|.
  static LayoutNGTextCombine* CreateAnonymous(LayoutText* text_child);

  // Returns true if |layout_object| is a child of |LayoutNGTextCombine|.
  static bool ShouldBeParentOf(const LayoutObject& layout_object);

 private:
  bool IsOfType(LayoutObjectType) const override;
  const char* GetName() const override { return "LayoutNGTextCombine"; }
};

// static
inline bool LayoutNGTextCombine::ShouldBeParentOf(
    const LayoutObject& layout_object) {
  if (LIKELY(layout_object.IsHorizontalWritingMode()) ||
      !layout_object.IsText())
    return false;
  return UNLIKELY(layout_object.StyleRef().HasTextCombine()) &&
         layout_object.IsLayoutNGObject();
}

template <>
struct DowncastTraits<LayoutNGTextCombine> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutNGTextCombine();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_COMBINE_H_
