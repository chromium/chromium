// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_ANIMATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_ANIMATOR_H_

#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"

namespace blink {

class StyleResolverState;
class StyleCascade;

namespace cssvalue {

class CSSPendingInterpolationValue;

}  // namespace cssvalue

// StyleAnimator is a class which knows how to apply active interpolations
// for given a CSSProperty.
//
// When the set of currently animating properties has been determined,
// a CSSPendingInterpolationValues is added to the cascade for each animating
// property (see StyleResolver::CascadeInterpolations). Later, when the
// cascade is applied, an Animator may be provided. That Animator is then
// responsible for applying the actual interpolated values represented by
// any CSSPendingInterpolationValues that may (or may not) remain in the
// cascade. See StyleCascade::Animator for more information.
//
// TODO(crbug.com/985051) Evaluate if there's a performance issue here, and
// if so possibly store active interpolations directly in the cascade.
class CORE_EXPORT StyleAnimator : public StyleCascade::Animator {
  STACK_ALLOCATED();

 public:
  explicit StyleAnimator(StyleResolverState&, StyleCascade&);

  void Apply(const CSSProperty&,
             const cssvalue::CSSPendingInterpolationValue&,
             StyleCascade::Resolver&) override;

 private:
  StyleResolverState& state_;
  StyleCascade& cascade_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_ANIMATOR_H_
