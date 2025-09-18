// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"

#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"

namespace blink {

const CSSValue* CSSInterpolationEnvironment::Resolve(
    const PropertyHandle& property,
    const CSSValue* value,
    const TreeScope* tree_scope) const {
  DCHECK(cascade_);
  DCHECK(cascade_resolver_);
  if (!value)
    return value;
  // TODO: If we support env() within @keyframe, we may need to support
  // non-nullptr env_bindings here.
  return cascade_->Resolve(property.GetCSSPropertyName(), *value, tree_scope,
                           /*env_bindings=*/nullptr, CascadeOrigin::kAnimation,
                           *cascade_resolver_);
}

}  // namespace blink
