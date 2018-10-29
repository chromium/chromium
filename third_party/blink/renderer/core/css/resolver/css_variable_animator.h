// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CSS_VARIABLE_ANIMATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CSS_VARIABLE_ANIMATOR_H_

#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"

namespace blink {

class StyleResolverState;
class CSSAnimationUpdate;

// CSSVariableAnimator is a special CSSVariableResolver which can apply
// animated values during var()-resolution. In other words, it makes sure that
// if a var()-reference to a currently animating custom property is encountered,
// we will first apply the animated value for that property before resolving it.
class CORE_EXPORT CSSVariableAnimator : public CSSVariableResolver {
  STACK_ALLOCATED();

 public:
  explicit CSSVariableAnimator(StyleResolverState&);

  // Apply all custom property animations. After calling this, the set of
  // pending properties will be empty and further calls to ApplyAll will have
  // no effect.
  void ApplyAll();

 protected:
  void ApplyAnimation(const AtomicString&) override;

 private:
  // Apply the animated value of a single property. The property must exist
  // in 'pending_properties_'.
  void Apply(const PropertyHandle&);

  StyleResolverState& state_;
  const CSSAnimationUpdate& update_;
  // Set of custom properties with pending animations. We will apply these
  // one by one until the set is empty.
  HashSet<PropertyHandle> pending_properties_;
};

}  // namespace blink

#endif  // CSSVariableAnimator
