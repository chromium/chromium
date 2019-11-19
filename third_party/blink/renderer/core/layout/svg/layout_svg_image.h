/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_IMAGE_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_model_object.h"

namespace blink {

class LayoutImageResource;
class SVGImageElement;

class LayoutSVGImage final : public LayoutSVGModelObject {
 public:
  explicit LayoutSVGImage(SVGImageElement*);
  ~LayoutSVGImage() override;

  void SetNeedsBoundariesUpdate() override { needs_boundaries_update_ = true; }
  void SetNeedsTransformUpdate() override { needs_transform_update_ = true; }

  LayoutImageResource* ImageResource() { return image_resource_.Get(); }
  const LayoutImageResource* ImageResource() const {
    return image_resource_.Get();
  }

  FloatRect ObjectBoundingBox() const override { return object_bounding_box_; }
  bool IsObjectBoundingBoxValid() const {
    return !object_bounding_box_.IsEmpty();
  }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGImage ||
           LayoutSVGModelObject::IsOfType(type);
  }

  const char* GetName() const override { return "LayoutSVGImage"; }

 protected:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void WillBeDestroyed() override;

 private:
  FloatRect StrokeBoundingBox() const override { return object_bounding_box_; }

  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) override;

  void UpdateLayout() override;
  void Paint(const PaintInfo&) const override;

  bool UpdateBoundingBox();

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;

  AffineTransform LocalSVGTransform() const override {
    return local_transform_;
  }

  FloatSize CalculateObjectSize() const;
  IntSize GetOverriddenIntrinsicSize() const;

  bool needs_boundaries_update_ : 1;
  bool needs_transform_update_ : 1;
  bool transform_uses_reference_box_ : 1;
  AffineTransform local_transform_;
  FloatRect object_bounding_box_;
  Persistent<LayoutImageResource> image_resource_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGImage, IsSVGImage());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_IMAGE_H_
