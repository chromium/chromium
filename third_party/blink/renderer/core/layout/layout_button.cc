/**
 * Copyright (C) 2005 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_button.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"

namespace blink {

LayoutButton::LayoutButton(Element* element)
    : LayoutFlexibleBox(element), inner_(nullptr) {}

LayoutButton::~LayoutButton() = default;

void LayoutButton::Trace(Visitor* visitor) const {
  visitor->Trace(inner_);
  LayoutFlexibleBox::Trace(visitor);
}

void LayoutButton::AddChild(LayoutObject* new_child,
                            LayoutObject* before_child) {
  NOT_DESTROYED();
  if (!inner_) {
    // Create an anonymous block.
    DCHECK(!FirstChild());
    inner_ = CreateAnonymousBlock(StyleRef().Display());
    LayoutFlexibleBox::AddChild(inner_);
  }

  inner_->AddChild(new_child, before_child);
}

void LayoutButton::RemoveChild(LayoutObject* old_child) {
  NOT_DESTROYED();
  if (old_child == inner_ || !inner_) {
    LayoutFlexibleBox::RemoveChild(old_child);
    inner_ = nullptr;

  } else if (old_child->Parent() == this) {
    // We aren't the inner node, but we're getting removed from the button, this
    // can happen with things like scrollable area resizer's.
    LayoutFlexibleBox::RemoveChild(old_child);

  } else {
    inner_->RemoveChild(old_child);
  }
}

void LayoutButton::UpdateAnonymousChildStyle(
    const LayoutObject* child,
    ComputedStyleBuilder& child_style_builder) const {
  DCHECK_EQ(inner_, child);
  UpdateAnonymousChildStyle(StyleRef(), child_style_builder);
}

// This function is shared with LayoutNGButton.
void LayoutButton::UpdateAnonymousChildStyle(
    const ComputedStyle& parent_style,
    ComputedStyleBuilder& child_style_builder) {
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

LayoutUnit LayoutButton::BaselinePosition(
    FontBaseline baseline,
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  NOT_DESTROYED();
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  // We want to call the LayoutBlock version of firstLineBoxBaseline to
  // avoid LayoutFlexibleBox synthesizing a baseline that we don't want.
  // We use this check as a proxy for "are there any line boxes in this button"
  if (!HasLineIfEmpty() && !ShouldApplyLayoutContainment() &&
      LayoutBlock::FirstLineBoxBaseline() == -1) {
    NOT_DESTROYED();
    // To ensure that we have a consistent baseline when we have no children,
    // even when we have the anonymous LayoutBlock child, we calculate the
    // baseline for the empty case manually here.
    if (direction == kHorizontalLine) {
      return MarginTop() + Size().Height() - BorderBottom() - PaddingBottom() -
             ComputeScrollbars().bottom;
    }
    return MarginRight() + Size().Width() - BorderLeft() - PaddingLeft() -
           ComputeScrollbars().left;
  }
  LayoutUnit result_baseline = LayoutFlexibleBox::BaselinePosition(
      baseline, first_line, direction, line_position_mode);
  // See crbug.com/690036 and crbug.com/304848.
  LayoutUnit correct_baseline = LayoutBlock::InlineBlockBaseline(direction);
  if (correct_baseline != result_baseline &&
      ShouldCountWrongBaseline(*this, StyleRef(),
                               Parent() ? Parent()->Style() : nullptr)) {
    for (LayoutBox* child = FirstChildBox(); child;
         child = child->NextSiblingBox()) {
      if (!child->IsFloatingOrOutOfFlowPositioned()) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kWrongBaselineOfMultiLineButton);
        return result_baseline;
      }
    }
    UseCounter::Count(GetDocument(),
                      WebFeature::kWrongBaselineOfEmptyLineButton);
  }
  return result_baseline;
}

bool LayoutButton::ShouldCountWrongBaseline(const LayoutBox& button_box,
                                            const ComputedStyle& style,
                                            const ComputedStyle* parent_style) {
  if (button_box.IsFloatingOrOutOfFlowPositioned())
    return false;
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
