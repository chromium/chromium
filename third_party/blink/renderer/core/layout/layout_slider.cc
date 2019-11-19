/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Publicw
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

#include "third_party/blink/renderer/core/layout/layout_slider.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/slider_thumb_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

const int LayoutSlider::kDefaultTrackLength = 129;

LayoutSlider::LayoutSlider(HTMLInputElement* element)
    : LayoutFlexibleBox(element) {
  // We assume LayoutSlider works only with <input type=range>.
  DCHECK_EQ(element->type(), input_type_names::kRange);
}

LayoutSlider::~LayoutSlider() = default;

LayoutUnit LayoutSlider::BaselinePosition(
    FontBaseline,
    bool /*firstLine*/,
    LineDirectionMode,
    LinePositionMode line_position_mode) const {
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  // FIXME: Patch this function for writing-mode.
  return Size().Height() + MarginTop();
}

void LayoutSlider::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  max_logical_width =
      LayoutUnit(kDefaultTrackLength * StyleRef().EffectiveZoom());
  if (!StyleRef().Width().IsPercentOrCalc())
    min_logical_width = max_logical_width;
}

inline SliderThumbElement* LayoutSlider::GetSliderThumbElement() const {
  return To<SliderThumbElement>(
      To<Element>(GetNode())->UserAgentShadowRoot()->getElementById(
          shadow_element_names::SliderThumb()));
}

bool LayoutSlider::InDragMode() const {
  return GetSliderThumbElement()->IsActive();
}

}  // namespace blink
