// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class CORE_EXPORT CSSAnimation : public Animation {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSAnimation* Create(AnimationEffect*,
                              AnimationTimeline*,
                              const String& animation_name);

  CSSAnimation(ExecutionContext*,
               AnimationTimeline*,
               AnimationEffect*,
               const String& animation_name);

  bool IsCSSAnimation() const final { return true; }

  const String& animationName() const { return animation_name_; }

 private:
  String animation_name_;
};

DEFINE_TYPE_CASTS(CSSAnimation,
                  Animation,
                  animation,
                  animation->IsCSSAnimation(),
                  animation.IsCSSAnimation());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_H_
