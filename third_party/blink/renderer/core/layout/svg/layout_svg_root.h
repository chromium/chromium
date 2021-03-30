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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_ROOT_H_

#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/svg/svg_content_container.h"

namespace blink {

class SVGElement;
enum class SVGTransformChange;

class CORE_EXPORT LayoutSVGRoot final : public LayoutReplaced {
 public:
  explicit LayoutSVGRoot(SVGElement*);
  ~LayoutSVGRoot() override;

  bool IsEmbeddedThroughSVGImage() const;
  bool IsEmbeddedThroughFrameContainingSVGDocument() const;

  void IntrinsicSizingInfoChanged();
  void UnscaledIntrinsicSizingInfo(IntrinsicSizingInfo&) const;

  // If you have a LayoutSVGRoot, use firstChild or lastChild instead.
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

  bool IsLayoutSizeChanged() const {
    NOT_DESTROYED();
    return is_layout_size_changed_;
  }
  bool DidScreenScaleFactorChange() const {
    NOT_DESTROYED();
    return did_screen_scale_factor_change_;
  }
  void SetNeedsBoundariesUpdate() override {
    NOT_DESTROYED();
    needs_boundaries_or_transform_update_ = true;
  }
  void SetNeedsTransformUpdate() override {
    NOT_DESTROYED();
    needs_boundaries_or_transform_update_ = true;
  }

  void SetContainerSize(const LayoutSize& container_size) {
    NOT_DESTROYED();
    // SVGImage::draw() does a view layout prior to painting,
    // and we need that layout to know of the new size otherwise
    // the layout may be incorrectly using the old size.
    if (container_size_ != container_size) {
      SetNeedsLayoutAndFullPaintInvalidation(
          layout_invalidation_reason::kSizeChanged);
    }
    container_size_ = container_size;
  }

  // localToBorderBoxTransform maps local SVG viewport coordinates to local CSS
  // box coordinates.
  const AffineTransform& LocalToBorderBoxTransform() const {
    NOT_DESTROYED();
    return local_to_border_box_transform_;
  }

  bool ShouldApplyViewportClip() const;

  void RecalcVisualOverflow() override;

  bool HasNonIsolatedBlendingDescendants() const final;

  bool HasDescendantCompositingReasons() const {
    NOT_DESTROYED();
    return AdditionalCompositingReasons() != CompositingReason::kNone;
  }
  void NotifyDescendantCompositingReasonsChanged();

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutSVGRoot";
  }

 private:
  OverflowClipAxes ComputeOverflowClipAxes() const override {
    NOT_DESTROYED();
    if (ShouldApplyViewportClip())
      return kOverflowClipBothAxis;
    return LayoutReplaced::ComputeOverflowClipAxes();
  }
  LayoutRect ComputeContentsVisualOverflow() const;

  LayoutObjectChildList* VirtualChildren() override {
    NOT_DESTROYED();
    return &content_.Children();
  }
  const LayoutObjectChildList* VirtualChildren() const override {
    NOT_DESTROYED();
    return &content_.Children();
  }

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectSVG || type == kLayoutObjectSVGRoot ||
           LayoutReplaced::IsOfType(type);
  }

  void ComputeIntrinsicSizingInfo(IntrinsicSizingInfo&) const override;
  LayoutUnit ComputeReplacedLogicalWidth(
      ShouldComputePreferred = kComputeActual) const override;
  LayoutUnit ComputeReplacedLogicalHeight(
      LayoutUnit estimated_used_width = LayoutUnit()) const override;
  void UpdateLayout() override;
  void PaintReplaced(const PaintInfo&,
                     const PhysicalOffset& paint_offset) const override;

  void WillBeDestroyed() override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;
  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;

  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;

  AffineTransform LocalToSVGParentTransform() const override;

  FloatRect ObjectBoundingBox() const override {
    NOT_DESTROYED();
    return content_.ObjectBoundingBox();
  }
  FloatRect StrokeBoundingBox() const override {
    NOT_DESTROYED();
    return content_.StrokeBoundingBox();
  }
  FloatRect VisualRectInLocalSVGCoordinates() const override {
    NOT_DESTROYED();
    return content_.StrokeBoundingBox();
  }

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;

  void MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                          TransformState&,
                          MapCoordinatesFlags) const override;

  bool CanHaveChildren() const override {
    NOT_DESTROYED();
    return true;
  }

  void DescendantIsolationRequirementsChanged(DescendantIsolationState) final;

  bool IntrinsicSizeIsFontMetricsDependent() const;
  bool StyleChangeAffectsIntrinsicSize(const ComputedStyle& old_style) const;

  void UpdateCachedBoundaries();
  SVGTransformChange BuildLocalToBorderBoxTransform();

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const final;

  // This is a special case for SVG documents with percentage dimensions which
  // would normally not change under zoom. See: https://crbug.com/222786.
  double LogicalSizeScaleFactorForPercentageLengths() const;

  PaintLayerType LayerTypeRequired() const override;
  bool CanHaveAdditionalCompositingReasons() const override {
    NOT_DESTROYED();
    return true;
  }
  CompositingReasons AdditionalCompositingReasons() const override;
  bool HasDescendantWithCompositingReason() const;

  SVGContentContainer content_;
  LayoutSize container_size_;
  AffineTransform local_to_border_box_transform_;
  bool is_layout_size_changed_ : 1;
  bool did_screen_scale_factor_change_ : 1;
  bool needs_boundaries_or_transform_update_ : 1;
  mutable bool has_non_isolated_blending_descendants_ : 1;
  mutable bool has_non_isolated_blending_descendants_dirty_ : 1;
  mutable bool has_descendant_with_compositing_reason_ : 1;
  mutable bool has_descendant_with_compositing_reason_dirty_ : 1;
};

template <>
struct DowncastTraits<LayoutSVGRoot> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsSVGRoot();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_ROOT_H_
