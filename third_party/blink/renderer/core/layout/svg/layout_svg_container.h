/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_CONTAINER_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_model_object.h"

namespace blink {

class SVGElement;
enum class SVGTransformChange;

class LayoutSVGContainer : public LayoutSVGModelObject {
 public:
  explicit LayoutSVGContainer(SVGElement*);
  ~LayoutSVGContainer() override;

  // If you have a LayoutSVGContainer, use firstChild or lastChild instead.
  void SlowFirstChild() const = delete;
  void SlowLastChild() const = delete;

  LayoutObject* FirstChild() const {
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->FirstChild();
  }
  LayoutObject* LastChild() const {
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->LastChild();
  }

  void Paint(const PaintInfo&) const override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void SetNeedsBoundariesUpdate() final { needs_boundaries_update_ = true; }
  bool DidScreenScaleFactorChange() const {
    return did_screen_scale_factor_change_;
  }
  bool IsObjectBoundingBoxValid() const { return object_bounding_box_valid_; }

  bool SelfWillPaint() const;

  bool HasNonIsolatedBlendingDescendants() const final;

  const char* GetName() const override { return "LayoutSVGContainer"; }

  FloatRect ObjectBoundingBox() const final { return object_bounding_box_; }

 protected:
  LayoutObjectChildList* VirtualChildren() final { return Children(); }
  const LayoutObjectChildList* VirtualChildren() const final {
    return Children();
  }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGContainer ||
           LayoutSVGModelObject::IsOfType(type);
  }
  void UpdateLayout() override;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) final;
  void RemoveChild(LayoutObject*) final;

  FloatRect StrokeBoundingBox() const final { return stroke_bounding_box_; }

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;

  // Called during layout to update the local transform.
  virtual SVGTransformChange CalculateLocalTransform();

  void UpdateCachedBoundaries();

  void DescendantIsolationRequirementsChanged(DescendantIsolationState) final;

 private:
  const LayoutObjectChildList* Children() const { return &children_; }
  LayoutObjectChildList* Children() { return &children_; }

  LayoutObjectChildList children_;
  FloatRect object_bounding_box_;
  FloatRect stroke_bounding_box_;
  bool object_bounding_box_valid_;
  bool needs_boundaries_update_ : 1;
  bool did_screen_scale_factor_change_ : 1;
  mutable bool has_non_isolated_blending_descendants_ : 1;
  mutable bool has_non_isolated_blending_descendants_dirty_ : 1;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGContainer, IsSVGContainer());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_CONTAINER_H_
