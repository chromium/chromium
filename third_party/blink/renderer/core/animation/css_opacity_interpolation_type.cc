// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_opacity_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

InterpolationValue CSSOpacityInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.IsForHighlight()) {
    if (const auto* identifier = DynamicTo<CSSIdentifierValue>(value);
        identifier && identifier->GetValueID() == CSSValueID::kAuto) {
      return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(1));
    }
  }
  return CSSNumberInterpolationType::MaybeConvertValue(value, state,
                                                       conversion_checkers);
}

}  // namespace blink
