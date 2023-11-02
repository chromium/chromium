/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_FRAME_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_FRAME_SET_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

class HTMLDimension;
class HTMLFrameSetElement;

class LayoutFrameSet final : public LayoutBox {
 public:
  explicit LayoutFrameSet(Element*);
  ~LayoutFrameSet() override;
  void Trace(Visitor*) const override;

  LayoutObject* FirstChild() const {
    NOT_DESTROYED();
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->FirstChild();
  }
  LayoutObject* LastChild() const {
    NOT_DESTROYED();
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->LastChild();
  }

  // If you have a LayoutFrameSet, use firstChild or lastChild instead.
  void SlowFirstChild() const = delete;
  void SlowLastChild() const = delete;

  const LayoutObjectChildList* Children() const {
    NOT_DESTROYED();
    return &children_;
  }
  LayoutObjectChildList* Children() {
    NOT_DESTROYED();
    return &children_;
  }

  HTMLFrameSetElement* FrameSet() const;

  class GridAxis {
    DISALLOW_NEW();

   public:
    GridAxis() = default;
    GridAxis(const GridAxis&) = delete;
    GridAxis& operator=(const GridAxis&) = delete;

    void Resize(int);

    Vector<LayoutUnit> sizes_;
    // A copy of HTMLFrameSetElement::allow_border_*.  It's helpful
    // because this information is used at the paint stage.
    Vector<bool> allow_border_;
  };

  const GridAxis& Rows() const {
    NOT_DESTROYED();
    return rows_;
  }
  const GridAxis& Columns() const {
    NOT_DESTROYED();
    return cols_;
  }

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutFrameSet";
  }

 private:
  LayoutObjectChildList* VirtualChildren() override {
    NOT_DESTROYED();
    return Children();
  }
  const LayoutObjectChildList* VirtualChildren() const override {
    NOT_DESTROYED();
    return Children();
  }

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectFrameSet || LayoutBox::IsOfType(type);
  }

  void UpdateLayout() override;
  void Paint(const PaintInfo&) const override;

  MinMaxSizes PreferredLogicalWidths() const override {
    NOT_DESTROYED();
    return MinMaxSizes();
  }
  MinMaxSizes ComputeIntrinsicLogicalWidths() const final {
    NOT_DESTROYED();
    MinMaxSizes sizes;
    LayoutUnit scrollbar_thickness = ComputeLogicalScrollbars().InlineSum();
    sizes += BorderAndPaddingLogicalWidth() + scrollbar_thickness;
    return sizes;
  }

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;
  void AddChild(LayoutObject* new_child, LayoutObject* before_child) override;
  void RemoveChild(LayoutObject* child) override;
  CursorDirective GetCursor(const PhysicalOffset&, ui::Cursor&) const override;

  void LayOutAxis(GridAxis&,
                  const Vector<HTMLDimension>&,
                  const Vector<int>& deltas,
                  int available_space);
  void PositionFrames();

  LayoutObjectChildList children_;

  GridAxis rows_;
  GridAxis cols_;
};

template <>
struct DowncastTraits<LayoutFrameSet> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsFrameSet();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_FRAME_SET_H_
