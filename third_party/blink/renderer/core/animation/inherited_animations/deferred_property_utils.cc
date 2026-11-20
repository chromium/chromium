// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/inherited_animations/deferred_property_utils.h"

#include "third_party/blink/renderer/core/style/style_animated_sources.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"

namespace blink {

cc::DeferredPropertyKey ToDeferredPropertyKey(const AnimatedSource& source,
                                              AnimatedSourceProperty property) {
  const cc::DeferredPropertyType type = ToDeferredPropertyType(property);
  if (!source.IsValid() || type == cc::DeferredPropertyType::kInvalid) {
    return {};
  }
  return {
      CompositorElementIdFromDOMNodeId(source.animated_source.GetDOMNodeId()),
      type};
}

}  // namespace blink
