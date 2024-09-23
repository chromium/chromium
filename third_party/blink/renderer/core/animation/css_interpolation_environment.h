// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_INTERPOLATION_ENVIRONMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_INTERPOLATION_ENVIRONMENT_H_

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

namespace blink {

class CascadeResolver;
class ComputedStyle;
class StyleCascade;

class CSSInterpolationEnvironment : public InterpolationEnvironment {
 public:
  CSSInterpolationEnvironment(const InterpolationTypesMap& map,
                              StyleResolverState& state,
                              StyleCascade* cascade,
                              CascadeResolver* cascade_resolver)
      : InterpolationEnvironment(map),
        state_(&state),
        base_style_(state.StyleBuilder().GetBaseComputedStyle()),
        animation_controls_style_(base_style_),
        cascade_(cascade),
        cascade_resolver_(cascade_resolver) {}

  CSSInterpolationEnvironment(const InterpolationTypesMap& map,
                              StyleResolverState& state)
      : InterpolationEnvironment(map), state_(&state) {}

  CSSInterpolationEnvironment(const InterpolationTypesMap& map,
                              const ComputedStyle& base_style,
                              const ComputedStyle& animation_controls_style)
      : InterpolationEnvironment(map),
        base_style_(&base_style),
        animation_controls_style_(&animation_controls_style) {}

  bool IsCSS() const final { return true; }

  StyleResolverState& GetState() {
    DCHECK(state_);
    return *state_;
  }
  const StyleResolverState& GetState() const {
    DCHECK(state_);
    return *state_;
  }

  StyleResolverState* GetOptionalState() { return state_; }
  const StyleResolverState* GetOptionalState() const { return state_; }

  const ComputedStyle& BaseStyle() const {
    DCHECK(base_style_);
    return *base_style_;
  }

  // This is the style that should be used for properties that control
  // animation behavior.  This is usually the same as BaseStyle, except in the
  // case of the interpolation environment used for the before-change style
  // for CSS transitions.  In that case, the AnimationControlsStyle() is the
  // after-change style.
  const ComputedStyle& AnimationControlsStyle() const {
    DCHECK(animation_controls_style_);
    return *animation_controls_style_;
  }

  // TODO(crbug.com/985023): This effective violates const.
  const CSSValue* Resolve(const PropertyHandle&, const CSSValue*) const;

 private:
  StyleResolverState* state_ = nullptr;
  const ComputedStyle* base_style_ = nullptr;
  const ComputedStyle* animation_controls_style_ = nullptr;
  StyleCascade* cascade_ = nullptr;
  CascadeResolver* cascade_resolver_ = nullptr;
};

template <>
struct DowncastTraits<CSSInterpolationEnvironment> {
  static bool AllowFrom(const InterpolationEnvironment& value) {
    return value.IsCSS();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_INTERPOLATION_ENVIRONMENT_H_
