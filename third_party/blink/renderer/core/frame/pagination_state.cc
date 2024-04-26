// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pagination_state.h"

#include "third_party/blink/renderer/core/layout/layout_ng_block_flow.h"

namespace blink {

PaginationState::PaginationState() = default;

void PaginationState::Trace(Visitor* visitor) const {
  visitor->Trace(anonymous_page_objects_);
}

LayoutBlockFlow* PaginationState::CreateAnonymousPageLayoutObject(
    Document& document,
    const ComputedStyle& style) {
  LayoutBlockFlow* block =
      LayoutNGBlockFlow::CreateAnonymous(&document, &style);
  block->SetIsDetachedNonDomRoot(true);
  anonymous_page_objects_.push_back(block);
  return block;
}

void PaginationState::DestroyAnonymousPageLayoutObjects() {
  for (LayoutObject* object : anonymous_page_objects_) {
    object->Destroy();
  }
  anonymous_page_objects_.clear();
}

}  // namespace blink
