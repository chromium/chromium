/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_RESOURCE_CLIPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_RESOURCE_CLIPPER_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/svg/svg_unit_types.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class SVGClipPathElement;

class LayoutSVGResourceClipper final : public LayoutSVGResourceContainer {
 public:
  explicit LayoutSVGResourceClipper(SVGClipPathElement*);
  ~LayoutSVGResourceClipper() override;

  const char* GetName() const override { return "LayoutSVGResourceClipper"; }

  void RemoveAllClientsFromCache(bool mark_for_invalidation = true) override;

  FloatRect ResourceBoundingBox(const FloatRect& reference_box);

  static const LayoutSVGResourceType kResourceType = kClipperResourceType;
  LayoutSVGResourceType ResourceType() const override { return kResourceType; }

  bool HitTestClipContent(const FloatRect&, const HitTestLocation&) const;

  SVGUnitTypes::SVGUnitType ClipPathUnits() const;
  AffineTransform CalculateClipTransform(const FloatRect& reference_box) const;

  base::Optional<Path> AsPath();
  sk_sp<const PaintRecord> CreatePaintRecord();

  bool HasCycle() { return in_clip_expansion_; }
  void BeginClipExpansion() {
    DCHECK(!in_clip_expansion_);
    in_clip_expansion_ = true;
  }
  void EndClipExpansion() {
    DCHECK(in_clip_expansion_);
    in_clip_expansion_ = false;
  }

 protected:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void WillBeDestroyed() override;

 private:
  void CalculateLocalClipBounds();

  // Cache of the clip path when using path clipping.
  enum ClipContentPathValidity {
    kClipContentPathUnknown,
    kClipContentPathValid,
    kClipContentPathInvalid
  } clip_content_path_validity_ = kClipContentPathUnknown;
  Path clip_content_path_;

  // Cache of the clip path paint record when falling back to masking for
  // clipping.
  sk_sp<const PaintRecord> cached_paint_record_;

  FloatRect local_clip_bounds_;

  // Reference cycle detection.
  bool in_clip_expansion_;
};

DEFINE_LAYOUT_SVG_RESOURCE_TYPE_CASTS(LayoutSVGResourceClipper,
                                      kClipperResourceType);

}  // namespace blink

#endif
