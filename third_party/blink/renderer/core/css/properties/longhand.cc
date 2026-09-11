// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhand.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

void Longhand::ApplyParentValue(StyleResolverState& state) const {
  // Creating the (computed) CSSValue involves unzooming using the parent's
  // effective zoom.
  const CSSValue* parent_computed_value =
      ComputedStyleUtils::ComputedPropertyValue(*this, *state.ParentStyle());
  CHECK(parent_computed_value);
  // Applying the CSSValue involves zooming using our effective zoom.
  ApplyValue(state, *parent_computed_value,
             static_cast<CSSProperty::ValueModeFlags>(ValueMode::kNormal));
  // The custom `ApplyValue` above clears the animated source, so copy it
  // again from the parent style.
  state.StyleBuilder().CopyAnimatedSourceFrom(
      PropertyID(), state.ParentStyle(),
      /*has_untracked_dependencies=*/true);
}

bool Longhand::ApplyParentValueIfZoomChanged(StyleResolverState& state) const {
  if (state.ParentStyle()->EffectiveZoom() !=
      state.StyleBuilder().EffectiveZoom()) {
    ApplyParentValue(state);
    return true;
  }
  return false;
}

}  // namespace blink
