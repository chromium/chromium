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

#include "base/check_op.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_model_object.h"
#include "third_party/blink/renderer/core/layout/svg/svg_content_container.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class SVGElement;
enum class SVGTransformChange;

class LayoutSVGContainer : public LayoutSVGModelObject {
 public:
  explicit LayoutSVGContainer(SVGElement*);
  ~LayoutSVGContainer() override;
  void Trace(Visitor*) const override;

  // If you have a LayoutSVGContainer, use firstChild or lastChild instead.
  void SlowFirstChild() const = delete;
  void SlowLastChild() const = delete;

  LayoutObject* FirstChild() const {
    NOT_DESTROYED();
    DCHECK_EQ(&content_.Children(), VirtualChildren());
    return content_.Children().FirstChild();
  }
  LayoutObject* LastChild() const {
    NOT_DESTROYED();
    DCHECK_EQ(&content_.Children(), VirtualChildren());
    return content_.Children().LastChild();
  }

  void Paint(const PaintInfo&) const override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void SetNeedsBoundariesUpdate() final {
    NOT_DESTROYED();
    needs_boundaries_update_ = true;
  }
  bool DidScreenScaleFactorChange() const {
    NOT_DESTROYED();
    return did_screen_scale_factor_change_;
  }
  bool IsObjectBoundingBoxValid() const {
    NOT_DESTROYED();
    return object_bounding_box_valid_;
  }

  bool HasNonIsolatedBlendingDescendants() const final;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutSVGContainer";
  }

  gfx::RectF ObjectBoundingBox() const final {
    NOT_DESTROYED();
    return content_.ObjectBoundingBox();
  }

 protected:
  LayoutObjectChildList* VirtualChildren() final {
    NOT_DESTROYED();
    return &content_.Children();
  }
  const LayoutObjectChildList* VirtualChildren() const final {
    NOT_DESTROYED();
    return &content_.Children();
  }
  SVGContentContainer& Content() {
    NOT_DESTROYED();
    return content_;
  }

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectSVGContainer ||
           LayoutSVGModelObject::IsOfType(type);
  }
  void UpdateLayout() override;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) final;
  void RemoveChild(LayoutObject*) final;

  gfx::RectF StrokeBoundingBox() const final {
    NOT_DESTROYED();
    return content_.StrokeBoundingBox();
  }

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestPhase) override;

  // Called during layout to update the local transform.
  virtual SVGTransformChange CalculateLocalTransform(bool bounds_changed);

  bool UpdateCachedBoundaries();

  void DescendantIsolationRequirementsChanged(DescendantIsolationState) final;

 private:
  SVGContentContainer content_;
  bool object_bounding_box_valid_;
  bool needs_boundaries_update_ : 1;
  bool did_screen_scale_factor_change_ : 1;
  mutable bool has_non_isolated_blending_descendants_ : 1;
  mutable bool has_non_isolated_blending_descendants_dirty_ : 1;
};

template <>
struct DowncastTraits<LayoutSVGContainer> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsSVGContainer();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_CONTAINER_H_
