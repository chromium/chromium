/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_slider_track.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"

namespace blink {

LayoutSliderTrack::LayoutSliderTrack(Element* element)
    : LayoutBlockFlow(element) {}

void LayoutSliderTrack::UpdateLayout() {
  NOT_DESTROYED();
  auto* input = To<HTMLInputElement>(GetNode()->OwnerShadowHost());
  const bool is_vertical = !StyleRef().IsHorizontalWritingMode();

  Element* thumb_element = input->UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdSliderThumb);
  LayoutBox* thumb = thumb_element ? thumb_element->GetLayoutBox() : nullptr;

  SubtreeLayoutScope layout_scope(*this);
  // Force a layout to reset the position of the thumb so the code below doesn't
  // move the thumb to the wrong place.
  // This is necessary for |web_tests/media/controls/
  // progress-bar-repaint-on-size-change.html|.
  if (thumb)
    layout_scope.SetChildNeedsLayout(thumb);

  LayoutBlockFlow::UpdateLayout();

  // These should always exist, unless someone mutates the shadow DOM (e.g., in
  // the inspector).
  if (!thumb)
    return;

  double percentage_offset = input->RatioValue().ToDouble();
  LayoutUnit available_extent = is_vertical ? ContentHeight() : ContentWidth();
  available_extent -=
      is_vertical ? thumb->Size().Height() : thumb->Size().Width();
  LayoutUnit offset(percentage_offset * available_extent);
  LayoutPoint thumb_location = thumb->Location();
  if (is_vertical) {
    thumb_location.SetY(thumb_location.Y() - offset);
  } else if (StyleRef().IsLeftToRightDirection()) {
    thumb_location.SetX(thumb_location.X() + offset);
  } else {
    thumb_location.SetX(thumb_location.X() - offset);
  }
  thumb->SetLocation(thumb_location);

  // We need one-off invalidation code here because painting of the timeline
  // element does not go through style.
  // Instead it has a custom implementation in C++ code.
  // Therefore the style system cannot understand when it needs to be paint
  // invalidated.
  Parent()->SetShouldDoFullPaintInvalidation();
}

}  // namespace blink
