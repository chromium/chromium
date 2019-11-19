// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_COMPOSITOR_KEYFRAME_DOUBLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_COMPOSITOR_KEYFRAME_DOUBLE_H_

#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class CORE_EXPORT CompositorKeyframeDouble final
    : public CompositorKeyframeValue {
 public:
  CompositorKeyframeDouble(double number) : number_(number) {}
  ~CompositorKeyframeDouble() override = default;

  static CompositorKeyframeDouble* Create(double number) {
    return MakeGarbageCollected<CompositorKeyframeDouble>(number);
  }

  double ToDouble() const { return number_; }

 private:
  Type GetType() const override { return Type::kDouble; }

  double number_;
};

DEFINE_COMPOSITOR_KEYFRAME_VALUE_TYPE_CASTS(CompositorKeyframeDouble,
                                            IsDouble());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_COMPOSITOR_KEYFRAME_DOUBLE_H_
