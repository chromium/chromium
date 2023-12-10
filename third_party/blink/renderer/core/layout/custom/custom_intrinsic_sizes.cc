// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_intrinsic_sizes.h"

#include "third_party/blink/renderer/core/layout/custom/custom_layout_child.h"

namespace blink {

CustomIntrinsicSizes::CustomIntrinsicSizes(CustomLayoutChild* child,
                                           CustomLayoutToken* token,
                                           double min_content_size,
                                           double max_content_size)
    : child_(child),
      token_(token),
      min_content_size_(min_content_size),
      max_content_size_(max_content_size) {}

const LayoutInputNode& CustomIntrinsicSizes::GetLayoutNode() const {
  return child_->GetLayoutNode();
}

void CustomIntrinsicSizes::Trace(Visitor* visitor) const {
  visitor->Trace(child_);
  visitor->Trace(token_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
