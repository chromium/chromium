// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/slider_track_element.h"

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

SliderTrackElement::SliderTrackElement(Document& document)
    : HTMLDivElement(document) {}

LayoutObject* SliderTrackElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutNGBlockFlow>(this);
}

}  // namespace blink
