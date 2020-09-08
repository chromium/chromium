/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_OVERFLOW_MODEL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_OVERFLOW_MODEL_H_

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

namespace blink {

inline void UniteLayoutOverflowRect(LayoutRect& layout_overflow,
                                    const LayoutRect& rect) {
  LayoutUnit max_x = std::max(rect.MaxX(), layout_overflow.MaxX());
  LayoutUnit max_y = std::max(rect.MaxY(), layout_overflow.MaxY());
  LayoutUnit min_x = std::min(rect.X(), layout_overflow.X());
  LayoutUnit min_y = std::min(rect.Y(), layout_overflow.Y());
  // In case the width/height is larger than LayoutUnit can represent, fix the
  // right/bottom edge and shift the top/left ones.
  layout_overflow.SetWidth(max_x - min_x);
  layout_overflow.SetHeight(max_y - min_y);
  layout_overflow.SetX(max_x - layout_overflow.Width());
  layout_overflow.SetY(max_y - layout_overflow.Height());
}

// OverflowModel classes track content that spills out of an object.
// SimpleOverflowModel is used by InlineFlowBox, and BoxOverflowModel is
// used by LayoutBox.
//
// SimpleOverflowModel tracks 2 types of overflows: layout and visual
// overflows. BoxOverflowModel separates visual overflow into self visual
// overflow and contents visual overflow.
//
// All overflows are in the coordinate space of the object (i.e. physical
// coordinates with flipped block-flow direction). See documentation of
// LayoutBoxModelObject and LayoutBox::noOverflowRect() for more details.
//
// The classes model the overflows as rectangles that unite all the sources of
// overflow. This is the natural choice for layout overflow (scrollbars are
// linear in nature, thus are modeled by rectangles in 2D). For visual overflow
// and content visual overflow, this is a first order simplification though as
// they can be thought of as a collection of (potentially overlapping)
// rectangles.
//
// Layout overflow is the overflow that is reachable via scrollbars. It is used
// to size the scrollbar thumb and determine its position, which is determined
// by the maximum layout overflow size.
// Layout overflow cannot occur without an overflow clip as this is the only way
// to get scrollbars. As its name implies, it is a direct consequence of layout.
// Example of layout overflow:
// * in the inline case, a tall image could spill out of a line box.
// * 'height' / 'width' set to a value smaller than the one needed by the
//   descendants.
// Due to how scrollbars work, no overflow in the logical top and logical left
// direction is allowed(see LayoutBox::addLayoutOverflow).
//
// Visual overflow covers all the effects that visually bleed out of the box.
// Its primary use is to determine the area to invalidate.
// Visual overflow includes ('text-shadow' / 'box-shadow'), text stroke,
// 'outline', 'border-image', etc.
//
// An overflow model object is allocated only when some of these fields have
// non-default values in the owning object. Care should be taken to use adder
// functions (addLayoutOverflow, addVisualOverflow, etc.) to keep this
// invariant.

class SimpleLayoutOverflowModel {
  USING_FAST_MALLOC(SimpleLayoutOverflowModel);

 public:
  SimpleLayoutOverflowModel(const LayoutRect& layout_rect)
      : layout_overflow_(layout_rect) {}
  SimpleLayoutOverflowModel(const SimpleLayoutOverflowModel&) = delete;
  SimpleLayoutOverflowModel& operator=(const SimpleLayoutOverflowModel&) =
      delete;

  const LayoutRect& LayoutOverflowRect() const { return layout_overflow_; }
  void SetLayoutOverflow(const LayoutRect& rect) { layout_overflow_ = rect; }
  void AddLayoutOverflow(const LayoutRect& rect) {
    UniteLayoutOverflowRect(layout_overflow_, rect);
  }

  void Move(LayoutUnit dx, LayoutUnit dy) { layout_overflow_.Move(dx, dy); }

 private:
  LayoutRect layout_overflow_;
};

class SimpleVisualOverflowModel {
  USING_FAST_MALLOC(SimpleVisualOverflowModel);

 public:
  SimpleVisualOverflowModel(const LayoutRect& visual_rect)
      : visual_overflow_(visual_rect) {}
  SimpleVisualOverflowModel(const SimpleVisualOverflowModel&) = delete;
  SimpleVisualOverflowModel& operator=(const SimpleVisualOverflowModel&) =
      delete;
  const LayoutRect& VisualOverflowRect() const { return visual_overflow_; }
  void SetVisualOverflow(const LayoutRect& rect) { visual_overflow_ = rect; }
  void AddVisualOverflow(const LayoutRect& rect) {
    visual_overflow_.Unite(rect);
  }

  void Move(LayoutUnit dx, LayoutUnit dy) {
    visual_overflow_.Move(dx, dy);
  }

 private:
  LayoutRect visual_overflow_;
};

struct SimpleOverflowModel {
  base::Optional<SimpleLayoutOverflowModel> layout_overflow;
  base::Optional<SimpleVisualOverflowModel> visual_overflow;
};

// BoxModelOverflow tracks overflows of a LayoutBox. It separates visual
// overflow into self visual overflow and contents visual overflow.
//
// Self visual overflow covers all the effects of the object itself that
// visually bleed out of the box.
//
// Content visual overflow includes anything that would bleed out of the box and
// would be clipped by the overflow clip ('overflow' != visible). This
// corresponds to children that overflows their parent.
// It's important to note that this overflow ignores descendants with
// self-painting layers (see the SELF-PAINTING LAYER section in PaintLayer).
// This is required by the simplification made by this model (single united
// rectangle) to avoid gigantic invalidation. A good example for this is
// positioned objects that can be anywhere on the page and could artificially
// inflate the visual overflow.
// The main use of content visual overflow is to prevent unneeded clipping in
// BoxPainter (see https://crbug.com/238732). Note that the code path for
// self-painting layer is handled by PaintLayerPainter, which relies on
// PaintLayerClipper and thus ignores this optimization.
//
// Visual overflow covers self visual overflow, and if the box doesn't clip
// overflow, also content visual overflow. OverflowModel doesn't keep visual
// overflow, but keeps self visual overflow and contents visual overflow
// separately. The box should use self visual overflow as visual overflow if
// it clips overflow, otherwise union of self visual overflow and contents
// visual overflow.

class BoxLayoutOverflowModel {
 public:
  BoxLayoutOverflowModel(const LayoutRect& layout_rect)
      : layout_overflow_(layout_rect) {}
  BoxLayoutOverflowModel(const BoxLayoutOverflowModel&) = delete;
  BoxLayoutOverflowModel& operator=(const BoxLayoutOverflowModel&) = delete;

  const LayoutRect& LayoutOverflowRect() const { return layout_overflow_; }
  void SetLayoutOverflow(const LayoutRect& rect) { layout_overflow_ = rect; }
  void AddLayoutOverflow(const LayoutRect& rect) {
    UniteLayoutOverflowRect(layout_overflow_, rect);
  }

  void Move(LayoutUnit dx, LayoutUnit dy) { layout_overflow_.Move(dx, dy); }

  LayoutUnit LayoutClientAfterEdge() const { return layout_client_after_edge_; }
  void SetLayoutClientAfterEdge(LayoutUnit client_after_edge) {
    layout_client_after_edge_ = client_after_edge;
  }

 private:
  LayoutRect layout_overflow_;
  LayoutUnit layout_client_after_edge_;
};

class BoxVisualOverflowModel {
 public:
  BoxVisualOverflowModel(const LayoutRect& self_visual_overflow_rect)
      : self_visual_overflow_(self_visual_overflow_rect) {}
  BoxVisualOverflowModel(const BoxVisualOverflowModel&) = delete;
  BoxVisualOverflowModel& operator=(const BoxVisualOverflowModel&) = delete;

  void SetSelfVisualOverflow(const LayoutRect& rect) {
    self_visual_overflow_ = rect;
  }

  const LayoutRect& SelfVisualOverflowRect() const {
    return self_visual_overflow_;
  }
  void AddSelfVisualOverflow(const LayoutRect& rect) {
    self_visual_overflow_.Unite(rect);
  }

  const LayoutRect& ContentsVisualOverflowRect() const {
    return contents_visual_overflow_;
  }
  void AddContentsVisualOverflow(const LayoutRect& rect) {
    contents_visual_overflow_.Unite(rect);
  }

  void Move(LayoutUnit dx, LayoutUnit dy) {
    self_visual_overflow_.Move(dx, dy);
    contents_visual_overflow_.Move(dx, dy);
  }

  void SetHasSubpixelVisualEffectOutsets(bool b) {
    has_subpixel_visual_effect_outsets_ = b;
  }
  bool HasSubpixelVisualEffectOutsets() const {
    return has_subpixel_visual_effect_outsets_;
  }

 private:
  LayoutRect self_visual_overflow_;
  LayoutRect contents_visual_overflow_;
  bool has_subpixel_visual_effect_outsets_ = false;
};

struct BoxOverflowModel {
  base::Optional<BoxLayoutOverflowModel> layout_overflow;
  base::Optional<BoxVisualOverflowModel> visual_overflow;

  // Used by BoxPaintInvalidator. Stores the previous overflow data after the
  // last paint invalidation.
  struct PreviousOverflowData {
    bool previously_had_non_visible_overflow = false;
    PhysicalRect previous_physical_layout_overflow_rect;
    PhysicalRect previous_physical_self_visual_overflow_rect;
  };
  base::Optional<PreviousOverflowData> previous_overflow_data;

  USING_FAST_MALLOC(BoxOverflowModel);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_OVERFLOW_MODEL_H_
