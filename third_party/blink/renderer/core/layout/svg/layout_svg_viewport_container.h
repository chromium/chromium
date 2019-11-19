/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_VIEWPORT_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_VIEWPORT_CONTAINER_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"

namespace blink {

class SVGSVGElement;

// This is used for non-root <svg> elements which are SVGTransformable thus we
// inherit from LayoutSVGContainer instead of LayoutSVGTransformableContainer.
class LayoutSVGViewportContainer final : public LayoutSVGContainer {
 public:
  explicit LayoutSVGViewportContainer(SVGSVGElement*);
  FloatRect Viewport() const { return viewport_; }

  bool IsLayoutSizeChanged() const { return is_layout_size_changed_; }

  void SetNeedsTransformUpdate() override;

  const char* GetName() const override { return "LayoutSVGViewportContainer"; }

  AffineTransform LocalToSVGParentTransform() const override {
    return local_to_parent_transform_;
  }

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGViewportContainer ||
           LayoutSVGContainer::IsOfType(type);
  }

  void UpdateLayout() override;

  SVGTransformChange CalculateLocalTransform() override;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) final;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  FloatRect viewport_;
  mutable AffineTransform local_to_parent_transform_;
  bool is_layout_size_changed_ : 1;
  bool needs_transform_update_ : 1;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGViewportContainer,
                                IsSVGViewportContainer());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_VIEWPORT_CONTAINER_H_
