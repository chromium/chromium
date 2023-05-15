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

#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"

namespace blink {

LayoutSVGTransformableContainer::LayoutSVGTransformableContainer(
    SVGGraphicsElement* node)
    : LayoutSVGContainer(node),
      needs_transform_update_(true),
      transform_uses_reference_box_(false) {}

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

void LayoutSVGTransformableContainer::SetNeedsTransformUpdate() {
  NOT_DESTROYED();
  // The transform paint property relies on the SVG transform being up-to-date
  // (see: PaintPropertyTreeBuilder::updateTransformForNonRootSVG).
  SetNeedsPaintPropertyUpdate();
  needs_transform_update_ = true;
}

SVGTransformChange LayoutSVGTransformableContainer::CalculateLocalTransform(
    bool bounds_changed) {
  NOT_DESTROYED();
  SVGElement* element = GetElement();
  DCHECK(element);

  // If we're the LayoutObject for a <use> element, this container needs to
  // respect the translations induced by their corresponding use elements x/y
  // attributes.
  if (IsA<SVGUseElement>(element)) {
    const ComputedStyle& style = StyleRef();
    SVGLengthContext length_context(element);
    gfx::Vector2dF translation =
        length_context.ResolveLengthPair(style.X(), style.Y(), style);
    // TODO(fs): Signal this on style update instead.
    if (translation != additional_translation_)
      SetNeedsTransformUpdate();
    additional_translation_ = translation;
  }

  if (!needs_transform_update_ && transform_uses_reference_box_) {
    if (CheckForImplicitTransformChange(bounds_changed))
      SetNeedsTransformUpdate();
  }

  if (!needs_transform_update_)
    return SVGTransformChange::kNone;

  SVGTransformChangeDetector change_detector(local_transform_);
  local_transform_ =
      element->CalculateTransform(SVGElement::kIncludeMotionTransform);
  local_transform_.Translate(additional_translation_.x(),
                             additional_translation_.y());
  needs_transform_update_ = false;
  return change_detector.ComputeChange(local_transform_);
}

void LayoutSVGTransformableContainer::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGContainer::StyleDidChange(diff, old_style);

  TransformHelper::UpdateOffsetPath(*GetElement(), old_style);
  transform_uses_reference_box_ =
      TransformHelper::DependsOnReferenceBox(StyleRef());
}

}  // namespace blink
