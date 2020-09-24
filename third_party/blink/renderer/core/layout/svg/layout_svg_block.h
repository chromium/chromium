/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_BLOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_BLOCK_H_

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

class SVGElement;

// A common class of SVG objects that delegate layout, paint, etc. tasks to
// LayoutBlockFlow. It has two coordinate spaces:
// - local SVG coordinate space: similar to LayoutSVGModelObject, the space
//   that localSVGTransform() applies.
// - local HTML coordinate space: defined by frameRect() as if the local SVG
//   coordinate space created a containing block. Like other LayoutBlockFlow
//   objects, LayoutSVGBlock's frameRect() is also in physical coordinates with
//   flipped blocks direction in the "containing block".
class LayoutSVGBlock : public LayoutBlockFlow {
 public:
  explicit LayoutSVGBlock(SVGElement*);

  // These mapping functions map coordinates in HTML spaces.
  void MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                          TransformState&,
                          MapCoordinatesFlags) const final;
  void MapAncestorToLocal(const LayoutBoxModelObject* ancestor,
                          TransformState&,
                          MapCoordinatesFlags) const final;
  const LayoutObject* PushMappingToContainer(
      const LayoutBoxModelObject* ancestor_to_stop_at,
      LayoutGeometryMap&) const final;

  AffineTransform LocalSVGTransform() const final { return local_transform_; }
  void SetNeedsTransformUpdate() override { needs_transform_update_ = true; }

  PaintLayerType LayerTypeRequired() const override { return kNoPaintLayer; }

  SVGElement* GetElement() const;

 protected:
  void WillBeDestroyed() override;
  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;

  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = kDefaultVisualRectFlags) const final;

  AffineTransform local_transform_;
  bool needs_transform_update_ : 1;
  bool transform_uses_reference_box_ : 1;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVG || LayoutBlockFlow::IsOfType(type);
  }

  bool CheckForImplicitTransformChange(bool bbox_changed) const;
  bool UpdateTransformAfterLayout(bool bounds_changed);
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

 private:
  // LayoutSVGBlock subclasses should use GetElement() instead.
  void GetNode() const = delete;

  PhysicalRect VisualRectInDocument(VisualRectFlags) const final;

  void UpdateFromStyle() final;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_BLOCK_H_
