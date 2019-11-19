/*
 * This file is part of the select element layoutObject in WebCore.
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_MENU_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_MENU_LIST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

namespace blink {

class HTMLOptionElement;
class HTMLSelectElement;
class LayoutText;

class CORE_EXPORT LayoutMenuList final : public LayoutFlexibleBox {
 public:
  explicit LayoutMenuList(Element*);
  ~LayoutMenuList() override;

  HTMLSelectElement* SelectElement() const;
  void DidSelectOption(HTMLOptionElement*);
  String GetText() const;

  const char* GetName() const override { return "LayoutMenuList"; }

  LayoutUnit ClientPaddingLeft() const;
  LayoutUnit ClientPaddingRight() const;

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectMenuList || LayoutFlexibleBox::IsOfType(type);
  }
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;
  bool CreatesAnonymousWrapper() const override { return true; }

  void UpdateFromElement() override;

  PhysicalRect ControlClipRect(const PhysicalOffset&) const override;
  bool HasControlClip() const override { return true; }

  void ComputeIntrinsicLogicalWidths(
      LayoutUnit& min_logical_width,
      LayoutUnit& max_logical_width) const override;
  void ComputeLogicalHeight(LayoutUnit logical_height,
                            LayoutUnit logical_top,
                            LogicalExtentComputedValues&) const override;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  bool HasLineIfEmpty() const override { return true; }

  // Flexbox defines baselines differently than regular blocks.
  // For backwards compatibility, menulists need to do the regular block
  // behavior.
  LayoutUnit BaselinePosition(FontBaseline baseline,
                              bool first_line,
                              LineDirectionMode direction,
                              LinePositionMode position) const override {
    return LayoutBlock::BaselinePosition(baseline, first_line, direction,
                                         position);
  }
  LayoutUnit FirstLineBoxBaseline() const override {
    return LayoutBlock::FirstLineBoxBaseline();
  }
  LayoutUnit InlineBlockBaseline(LineDirectionMode direction) const override {
    return LayoutBlock::InlineBlockBaseline(direction);
  }

  void CreateInnerBlock();
  scoped_refptr<ComputedStyle> CreateInnerStyle();
  void UpdateInnerStyle();
  void AdjustInnerStyle(ComputedStyle&) const;
  bool HasOptionStyleChanged(const ComputedStyle& inner_style) const;
  void SetText(const String&);
  void UpdateInnerBlockHeight();
  void UpdateOptionsWidth() const;
  void SetIndexToSelectOnCancel(int list_index);

  void DidUpdateActiveOption(HTMLOptionElement*);

  LayoutText* button_text_;
  LayoutBlock* inner_block_;

  bool is_empty_ : 1;
  bool has_updated_active_option_ : 1;
  LayoutUnit inner_block_height_;
  // m_optionsWidth is calculated and cached on demand.
  // updateOptionsWidth() should be called before reading them.
  mutable int options_width_;

  int last_active_index_;

  scoped_refptr<const ComputedStyle> option_style_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutMenuList, IsMenuList());

}  // namespace blink

#endif
