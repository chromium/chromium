/*
 * This file is part of the layout object implementation for KHTML.
 *
 * Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DEPRECATED_FLEXIBLE_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DEPRECATED_FLEXIBLE_BOX_H_

#include "third_party/blink/renderer/core/layout/layout_block.h"

namespace blink {

// Handles layout for 'webkit-box' and 'webkit-inline-box'. This class will
// eventually be replaced by LayoutFlexibleBox.
class LayoutDeprecatedFlexibleBox final : public LayoutBlock {
 public:
  LayoutDeprecatedFlexibleBox(Element* element);
  ~LayoutDeprecatedFlexibleBox() override;

  const char* GetName() const override { return "LayoutDeprecatedFlexibleBox"; }

  void UpdateBlockLayout(bool relayout_children) override;
  void LayoutVerticalBox(bool relayout_children);

  bool IsDeprecatedFlexibleBox() const override { return true; }
  bool IsFlexibleBoxIncludingDeprecatedAndNG() const override { return true; }

 private:
  MinMaxSizes ComputeIntrinsicLogicalWidths() const override;

  void ApplyLineClamp(bool relayout_children);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DEPRECATED_FLEXIBLE_BOX_H_
