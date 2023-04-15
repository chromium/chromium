// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_INLINE_LIST_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_INLINE_LIST_ITEM_H_

#include "third_party/blink/renderer/core/layout/layout_inline.h"

namespace blink {

// A LayoutObject subclass for 'display: inline list-item'.
class LayoutNGInlineListItem final : public LayoutInline {
 public:
  explicit LayoutNGInlineListItem(Element* element);

 private:
  const char* GetName() const override;
  bool IsOfType(LayoutObjectType) const override;
};

template <>
struct DowncastTraits<LayoutNGInlineListItem> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsInlineListItem();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_INLINE_LIST_ITEM_H_
