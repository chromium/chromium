// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_button.h"

#include "third_party/blink/renderer/core/layout/layout_button.h"

namespace blink {

LayoutNGButton::LayoutNGButton(Element* element)
    : LayoutNGFlexibleBox(element), inner_(nullptr) {}

LayoutNGButton::~LayoutNGButton() = default;

void LayoutNGButton::Trace(Visitor* visitor) const {
  visitor->Trace(inner_);
  LayoutNGFlexibleBox::Trace(visitor);
}

void LayoutNGButton::AddChild(LayoutObject* new_child,
                              LayoutObject* before_child) {
  if (!inner_) {
    // Create an anonymous block.
    DCHECK(!FirstChild());
    inner_ = CreateAnonymousBlock(StyleRef().Display());
    LayoutNGFlexibleBox::AddChild(inner_);
  }

  inner_->AddChild(new_child, before_child);
}

void LayoutNGButton::RemoveChild(LayoutObject* old_child) {
  if (old_child == inner_ || !inner_) {
    LayoutNGFlexibleBox::RemoveChild(old_child);
    inner_ = nullptr;

  } else if (old_child->Parent() == this) {
    // We aren't the inner node, but we're getting removed from the button, this
    // can happen with things like scrollable area resizer's.
    LayoutNGFlexibleBox::RemoveChild(old_child);

  } else {
    inner_->RemoveChild(old_child);
  }
}

void LayoutNGButton::UpdateAnonymousChildStyle(
    const LayoutObject* child,
    ComputedStyleBuilder& child_style_builder) const {
  DCHECK_EQ(inner_, child);
  LayoutButton::UpdateAnonymousChildStyle(StyleRef(), child_style_builder);
}

}  // namespace blink
