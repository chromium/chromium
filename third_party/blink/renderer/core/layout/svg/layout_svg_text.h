/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_TEXT_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_block.h"

namespace blink {

class LayoutSVGInlineText;
class SVGTextElement;

class LayoutSVGText final : public LayoutSVGBlock {
 public:
  explicit LayoutSVGText(SVGTextElement*);
  ~LayoutSVGText() override;

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  void SetNeedsPositioningValuesUpdate() {
    needs_positioning_values_update_ = true;
  }
  void SetNeedsTransformUpdate() override { needs_transform_update_ = true; }
  void SetNeedsTextMetricsUpdate() { needs_text_metrics_update_ = true; }
  FloatRect VisualRectInLocalSVGCoordinates() const override;
  FloatRect ObjectBoundingBox() const override;
  FloatRect StrokeBoundingBox() const override;
  bool IsObjectBoundingBoxValid() const;

  void AddOutlineRects(Vector<PhysicalRect>&,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const override;

  static LayoutSVGText* LocateLayoutSVGTextAncestor(LayoutObject*);
  static const LayoutSVGText* LocateLayoutSVGTextAncestor(const LayoutObject*);

  bool NeedsReordering() const { return needs_reordering_; }
  const Vector<LayoutSVGInlineText*>& DescendantTextNodes() const {
    return descendant_text_nodes_;
  }

  void SubtreeChildWasAdded();
  void SubtreeChildWillBeRemoved();
  void SubtreeTextDidChange();

  void RecalcVisualOverflow() override;

  const char* GetName() const override { return "LayoutSVGText"; }

 private:
  bool AllowsOverflowClip() const override { return false; }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGText || LayoutSVGBlock::IsOfType(type);
  }

  void Paint(const PaintInfo&) const override;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;
  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  void UpdateLayout() override;

  void AbsoluteQuads(Vector<FloatQuad>&,
                     MapCoordinatesFlags mode = 0) const override;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void WillBeDestroyed() override;

  RootInlineBox* CreateRootInlineBox() override;

  void InvalidatePositioningValues(LayoutInvalidationReasonForTracing);

  bool needs_reordering_ : 1;
  bool needs_positioning_values_update_ : 1;
  bool needs_transform_update_ : 1;
  bool needs_text_metrics_update_ : 1;
  Vector<LayoutSVGInlineText*> descendant_text_nodes_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGText, IsSVGText());

}  // namespace blink

#endif
