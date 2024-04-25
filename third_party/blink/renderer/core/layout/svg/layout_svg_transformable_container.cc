/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"

#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/svg/svg_a_element.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/core/svg/svg_switch_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"

namespace blink {

LayoutSVGTransformableContainer::LayoutSVGTransformableContainer(
    SVGGraphicsElement* node)
    : LayoutSVGContainer(node) {}

static bool HasValidPredecessor(const Node* node) {
  DCHECK(node);
  for (node = node->previousSibling(); node; node = node->previousSibling()) {
    auto* svg_element = DynamicTo<SVGElement>(node);
    if (svg_element && svg_element->IsValid())
      return true;
  }
  return false;
}

bool LayoutSVGTransformableContainer::IsChildAllowed(
    LayoutObject* child,
    const ComputedStyle& style) const {
  NOT_DESTROYED();
  DCHECK(GetElement());
  Node* child_node = child->GetNode();
  if (IsA<SVGSwitchElement>(*GetElement())) {
    // Reject non-SVG/non-valid elements.
    auto* svg_element = DynamicTo<SVGElement>(child_node);
    if (!svg_element || !svg_element->IsValid()) {
      return false;
    }
    // Reject this child if it isn't the first valid node.
    if (HasValidPredecessor(child_node))
      return false;
  } else if (IsA<SVGAElement>(*GetElement())) {
    // http://www.w3.org/2003/01/REC-SVG11-20030114-errata#linking-text-environment
    // The 'a' element may contain any element that its parent may contain,
    // except itself.
    if (child_node && IsA<SVGAElement>(*child_node))
      return false;
    if (Parent() && Parent()->IsSVG())
      return Parent()->IsChildAllowed(child, style);
  }
  return LayoutSVGContainer::IsChildAllowed(child, style);
}

SVGTransformChange LayoutSVGTransformableContainer::UpdateLocalTransform(
    const gfx::RectF& reference_box) {
  NOT_DESTROYED();
  SVGElement* element = GetElement();
  DCHECK(element);
  // If we're the LayoutObject for a <use> element, this container needs to
  // respect the translations induced by their corresponding use elements x/y
  // attributes.
  if (IsA<SVGUseElement>(element)) {
    const ComputedStyle& style = StyleRef();
    const SVGViewportResolver viewport_resolver(*this);
    additional_translation_ =
        VectorForLengthPair(style.X(), style.Y(), viewport_resolver, style);
  }

  SVGTransformChangeDetector change_detector(local_transform_);
  local_transform_ = TransformHelper::ComputeTransformIncludingMotion(
      *GetElement(), reference_box);
  local_transform_.Translate(additional_translation_.x(),
                             additional_translation_.y());
  return change_detector.ComputeChange(local_transform_);
}

void LayoutSVGTransformableContainer::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGContainer::StyleDidChange(diff, old_style);

  // Check for changes to the 'x' or 'y' properties if this is a <use> element.
  SVGElement& element = *GetElement();
  if (old_style && IsA<SVGUseElement>(element)) {
    const ComputedStyle& style = StyleRef();
    if (old_style->X() != style.X() || old_style->Y() != style.Y()) {
      SetNeedsTransformUpdate();
    }
    // Any descendant could use context-fill or context-stroke, so we must
    // repaint the whole subtree.
    if (old_style->FillPaint() != style.FillPaint() ||
        old_style->StrokePaint() != style.StrokePaint()) {
      SetSubtreeShouldDoFullPaintInvalidation(
          PaintInvalidationReason::kSVGResource);
    }
  }

  // To support context-fill and context-stroke
  if (IsA<SVGUseElement>(element)) {
    SVGResources::UpdatePaints(*this, old_style, StyleRef());
  }

  TransformHelper::UpdateOffsetPath(element, old_style);
  SetTransformUsesReferenceBox(
      TransformHelper::UpdateReferenceBoxDependency(*this));
}

void LayoutSVGTransformableContainer::WillBeDestroyed() {
  if (IsA<SVGUseElement>(GetElement())) {
    SVGResources::ClearPaints(*this, Style());
  }
  LayoutSVGContainer::WillBeDestroyed();
}

}  // namespace blink
