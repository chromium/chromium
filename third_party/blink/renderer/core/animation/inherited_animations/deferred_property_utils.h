// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INHERITED_ANIMATIONS_DEFERRED_PROPERTY_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INHERITED_ANIMATIONS_DEFERRED_PROPERTY_UTILS_H_

#include "cc/paint/deferred_property_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"

namespace blink {

struct AnimatedSource;

// Returns the `cc::DeferredPropertyType` for `property`, or `kInvalid` if its
// animated values cannot be swapped in at raster time.
constexpr cc::DeferredPropertyType ToDeferredPropertyType(
    AnimatedSourceProperty property) {
  switch (property) {
    case AnimatedSourceProperty::kBackgroundColor:
      return cc::DeferredPropertyType::kBackgroundColor;
    default:
      return cc::DeferredPropertyType::kInvalid;
  }
}

// Returns the key that raster uses to look up `property`'s animated value on
// `source`, or an empty key when none applies.
CORE_EXPORT cc::DeferredPropertyKey ToDeferredPropertyKey(
    const AnimatedSource& source,
    AnimatedSourceProperty property);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INHERITED_ANIMATIONS_DEFERRED_PROPERTY_UTILS_H_
