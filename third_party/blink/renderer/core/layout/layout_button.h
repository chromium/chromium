/*
 * Copyright (C) 2005 Apple Computer
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BUTTON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BUTTON_H_

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"

namespace blink {

// LayoutButtons are just like normal flexboxes except that they will generate
// an anonymous block child.
// For inputs, they will also generate an anonymous LayoutText and keep its
// style and content up to date as the button changes.
class LayoutButton final : public LayoutFlexibleBox {
 public:
  explicit LayoutButton(Element*);
  ~LayoutButton() override;

  const char* GetName() const override { return "LayoutButton"; }
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutButton ||
           LayoutFlexibleBox::IsOfType(type);
  }

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;
  void RemoveLeftoverAnonymousBlock(LayoutBlock*) override {}
  bool CreatesAnonymousWrapper() const override { return true; }

  LayoutUnit BaselinePosition(FontBaseline,
                              bool first_line,
                              LineDirectionMode,
                              LinePositionMode) const override;

  static void UpdateAnonymousChildStyle(const ComputedStyle& parent_sytle,
                                        ComputedStyle& child_style);
  static bool ShouldCountWrongBaseline(const ComputedStyle& style,
                                       const ComputedStyle* parent_style);

 private:
  void UpdateAnonymousChildStyle(const LayoutObject* child,
                                 ComputedStyle& child_style) const override;

  LayoutBlock* inner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BUTTON_H_
