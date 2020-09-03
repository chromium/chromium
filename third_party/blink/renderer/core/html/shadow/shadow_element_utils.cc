// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"

namespace blink {

bool IsSliderContainer(const Element& element) {
  if (!element.IsInUserAgentShadowRoot())
    return false;
  const AtomicString& shadow_pseudo = element.ShadowPseudoId();
  return shadow_pseudo == "-webkit-media-slider-container" ||
         shadow_pseudo == "-webkit-slider-container";
}

}  // namespace blink
