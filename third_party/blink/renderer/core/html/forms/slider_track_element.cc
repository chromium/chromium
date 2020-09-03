// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/slider_track_element.h"

namespace blink {

SliderTrackElement::SliderTrackElement(Document& document)
    : HTMLDivElement(document) {}

LayoutObject* SliderTrackElement::CreateLayoutObject(const ComputedStyle& style,
                                                     LegacyLayout legacy) {
  // TODO(crbug.com/1040826): Introduce LayoutSliderTrack, and use it here.
  return HTMLDivElement::CreateLayoutObject(style, legacy);
}

}  // namespace blink
