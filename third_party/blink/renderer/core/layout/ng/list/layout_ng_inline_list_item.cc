// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inline_list_item.h"

namespace blink {

LayoutNGInlineListItem::LayoutNGInlineListItem(Element* element)
    : LayoutInline(element) {}

const char* LayoutNGInlineListItem::GetName() const {
  NOT_DESTROYED();
  return "LayoutNGInlineListItem";
}

bool LayoutNGInlineListItem::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGInlineListItem || LayoutInline::IsOfType(type);
}

}  // namespace blink
