// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_INTERPOLATION_ENVIRONMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_INTERPOLATION_ENVIRONMENT_H_

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

namespace blink {

class ComputedStyle;
class CSSVariableResolver;

class CSSInterpolationEnvironment : public InterpolationEnvironment {
 public:
  explicit CSSInterpolationEnvironment(const InterpolationTypesMap& map,
                                       StyleResolverState& state,
                                       CSSVariableResolver* variable_resolver)
      : InterpolationEnvironment(map),
        state_(&state),
        style_(state.Style()),
        variable_resolver_(variable_resolver) {}

  explicit CSSInterpolationEnvironment(const InterpolationTypesMap& map,
                                       StyleResolverState& state,
                                       StyleCascade* cascade,
                                       StyleCascade::Resolver* cascade_resolver)
      : InterpolationEnvironment(map),
        state_(&state),
        style_(state.Style()),
        cascade_(cascade),
        cascade_resolver_(cascade_resolver) {}

  explicit CSSInterpolationEnvironment(const InterpolationTypesMap& map,
                                       const ComputedStyle& style)
      : InterpolationEnvironment(map), style_(&style) {}

  bool IsCSS() const final { return true; }

  StyleResolverState& GetState() {
    DCHECK(state_);
    return *state_;
  }
  const StyleResolverState& GetState() const {
    DCHECK(state_);
    return *state_;
  }

  const ComputedStyle& Style() const {
    DCHECK(style_);
    return *style_;
  }

  bool HasVariableResolver() const {
    DCHECK(!RuntimeEnabledFeatures::CSSCascadeEnabled());
    return variable_resolver_;
  }

  // TODO(crbug.com/985023): This effective violates const.
  CSSVariableResolver& VariableResolver() const {
    DCHECK(!RuntimeEnabledFeatures::CSSCascadeEnabled());
    DCHECK(HasVariableResolver());
    return *variable_resolver_;
  }

  // TODO(crbug.com/985023): This effective violates const.
  const CSSValue* Resolve(const PropertyHandle& property,
                          const CSSValue* value) const {
    DCHECK(RuntimeEnabledFeatures::CSSCascadeEnabled());
    DCHECK(cascade_);
    DCHECK(cascade_resolver_);
    if (!value)
      return value;
    return cascade_->Resolve(property.GetCSSPropertyName(), *value,
                             *cascade_resolver_);
  }

 private:
  StyleResolverState* state_ = nullptr;
  const ComputedStyle* style_ = nullptr;
  CSSVariableResolver* variable_resolver_ = nullptr;
  StyleCascade* cascade_ = nullptr;
  StyleCascade::Resolver* cascade_resolver_ = nullptr;
};

DEFINE_TYPE_CASTS(CSSInterpolationEnvironment,
                  InterpolationEnvironment,
                  value,
                  value->IsCSS(),
                  value.IsCSS());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_INTERPOLATION_ENVIRONMENT_H_
