// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/forms/layout_button.h"

namespace blink {

LayoutButton::LayoutButton(Element* element)
    : LayoutNGFlexibleBox(element), inner_(nullptr) {}

LayoutButton::~LayoutButton() = default;

void LayoutButton::Trace(Visitor* visitor) const {
  visitor->Trace(inner_);
  LayoutNGFlexibleBox::Trace(visitor);
}

void LayoutButton::AddChild(LayoutObject* new_child,
                            LayoutObject* before_child) {
  if (!inner_) {
    // Create an anonymous block.
    DCHECK(!FirstChild());
    inner_ = CreateAnonymousBlock(StyleRef().Display());
    LayoutNGFlexibleBox::AddChild(inner_);
  }

  inner_->AddChild(new_child, before_child);
}

void LayoutButton::RemoveChild(LayoutObject* old_child) {
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

void LayoutButton::UpdateAnonymousChildStyle(
    const LayoutObject* child,
    ComputedStyleBuilder& child_style_builder) const {
  DCHECK_EQ(inner_, child);
  const ComputedStyle& parent_style = StyleRef();
  child_style_builder.SetFlexGrow(1.0f);
  // min-width: 0; is needed for correct shrinking.
  child_style_builder.SetMinWidth(Length::Fixed(0));
  // Use margin:auto instead of align-items:center to get safe centering, i.e.
  // when the content overflows, treat it the same as align-items: flex-start.
  child_style_builder.SetMarginTop(Length());
  child_style_builder.SetMarginBottom(Length());
  child_style_builder.SetFlexDirection(parent_style.FlexDirection());
  child_style_builder.SetJustifyContent(parent_style.JustifyContent());
  child_style_builder.SetFlexWrap(parent_style.FlexWrap());
  // TODO (lajava): An anonymous box must not be used to resolve children's auto
  // values.
  child_style_builder.SetAlignItems(parent_style.AlignItems());
  child_style_builder.SetAlignContent(parent_style.AlignContent());
}

bool LayoutButton::ShouldCountWrongBaseline(const LayoutBox& button_box,
                                            const ComputedStyle& style,
                                            const ComputedStyle* parent_style) {
  if (button_box.IsFloatingOrOutOfFlowPositioned()) {
    return false;
  }
  if (parent_style) {
    EDisplay display = parent_style->Display();
    if (display == EDisplay::kFlex || display == EDisplay::kInlineFlex ||
        display == EDisplay::kGrid || display == EDisplay::kInlineGrid) {
      StyleSelfAlignmentData alignment =
          style.ResolvedAlignSelf(ItemPosition::kAuto, parent_style);
      return alignment.GetPosition() == ItemPosition::kBaseline ||
             alignment.GetPosition() == ItemPosition::kLastBaseline;
    }
  }
  EVerticalAlign align = style.VerticalAlign();
  return align == EVerticalAlign::kBaseline ||
         align == EVerticalAlign::kBaselineMiddle ||
         align == EVerticalAlign::kSub || align == EVerticalAlign::kSuper ||
         align == EVerticalAlign::kLength;
}

}  // namespace blink
