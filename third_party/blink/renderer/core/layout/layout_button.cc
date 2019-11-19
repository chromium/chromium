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

namespace blink {

LayoutButton::LayoutButton(Element* element)
    : LayoutFlexibleBox(element), inner_(nullptr) {}

LayoutButton::~LayoutButton() = default;

void LayoutButton::AddChild(LayoutObject* new_child,
                            LayoutObject* before_child) {
  if (!inner_) {
    // Create an anonymous block.
    DCHECK(!FirstChild());
    inner_ = CreateAnonymousBlock(StyleRef().Display());
    LayoutFlexibleBox::AddChild(inner_);
  }

  inner_->AddChild(new_child, before_child);
}

void LayoutButton::RemoveChild(LayoutObject* old_child) {
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

void LayoutButton::UpdateAnonymousChildStyle(const LayoutObject* child,
                                             ComputedStyle& child_style) const {
  DCHECK_EQ(inner_, child);
  child_style.SetFlexGrow(1.0f);
  // min-width: 0; is needed for correct shrinking.
  child_style.SetMinWidth(Length::Fixed(0));
  // Use margin:auto instead of align-items:center to get safe centering, i.e.
  // when the content overflows, treat it the same as align-items: flex-start.
  child_style.SetMarginTop(Length());
  child_style.SetMarginBottom(Length());
  child_style.SetFlexDirection(StyleRef().FlexDirection());
  child_style.SetJustifyContent(StyleRef().JustifyContent());
  child_style.SetFlexWrap(StyleRef().FlexWrap());
  // TODO (lajava): An anonymous box must not be used to resolve children's auto
  // values.
  child_style.SetAlignItems(StyleRef().AlignItems());
  child_style.SetAlignContent(StyleRef().AlignContent());
}

PhysicalRect LayoutButton::ControlClipRect(
    const PhysicalOffset& additional_offset) const {
  // Clip to the padding box to at least give content the extra padding space.
  PhysicalRect rect(additional_offset, Size());
  rect.Expand(BorderInsets());
  return rect;
}

LayoutUnit LayoutButton::BaselinePosition(
    FontBaseline baseline,
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  // We want to call the LayoutBlock version of firstLineBoxBaseline to
  // avoid LayoutFlexibleBox synthesizing a baseline that we don't want.
  // We use this check as a proxy for "are there any line boxes in this button"
  if (!HasLineIfEmpty() && !ShouldApplyLayoutContainment() &&
      LayoutBlock::FirstLineBoxBaseline() == -1) {
    // To ensure that we have a consistent baseline when we have no children,
    // even when we have the anonymous LayoutBlock child, we calculate the
    // baseline for the empty case manually here.
    if (direction == kHorizontalLine) {
      return MarginTop() + Size().Height() - BorderBottom() - PaddingBottom() -
             HorizontalScrollbarHeight();
    }
    return MarginRight() + Size().Width() - BorderLeft() - PaddingLeft() -
           VerticalScrollbarWidth();
  }
  return LayoutFlexibleBox::BaselinePosition(baseline, first_line, direction,
                                             line_position_mode);
}

// For compatibility with IE/FF we only clip overflow on input elements.
bool LayoutButton::HasControlClip() const {
  return !IsA<HTMLButtonElement>(GetNode());
}
}  // namespace blink
