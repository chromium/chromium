/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 *
 */

#include "third_party/blink/renderer/modules/accessibility/ax_progress_indicator.h"

#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

// We can't assume that layout_object is always a `LayoutProgress`.
// Depending on CSS styles, a <progress> element may
// instead have a generic `LayoutObject`.
// See the `HTMLProgressElement` class for more information.
AXProgressIndicator::AXProgressIndicator(LayoutObject* layout_object,
                                         AXObjectCacheImpl& ax_object_cache)
    : AXNodeObject(layout_object, ax_object_cache) {
  DCHECK(layout_object);
  DCHECK(IsA<HTMLProgressElement>(layout_object->GetNode()))
      << "The layout object's node isn't an HTMLProgressElement.";
}

ax::mojom::blink::Role AXProgressIndicator::NativeRoleIgnoringAria() const {
  return ax::mojom::blink::Role::kProgressIndicator;
}

bool AXProgressIndicator::ValueForRange(float* out_value) const {
  if (AriaFloatAttribute(html_names::kAriaValuenowAttr, out_value)) {
    return true;
  }

  if (GetProgressElement()->position() >= 0) {
    *out_value = ClampTo<float>(GetProgressElement()->value());
    return true;
  }
  // Indeterminate progress bar has no value.
  return false;
}

bool AXProgressIndicator::MaxValueForRange(float* out_value) const {
  if (AriaFloatAttribute(html_names::kAriaValuemaxAttr, out_value)) {
    return true;
  }

  *out_value = ClampTo<float>(GetProgressElement()->max());
  return true;
}

bool AXProgressIndicator::MinValueForRange(float* out_value) const {
  if (AriaFloatAttribute(html_names::kAriaValueminAttr, out_value)) {
    return true;
  }

  *out_value = 0.0f;
  return true;
}

HTMLProgressElement* AXProgressIndicator::GetProgressElement() const {
  return DynamicTo<HTMLProgressElement>(GetNode());
}

}  // namespace blink
