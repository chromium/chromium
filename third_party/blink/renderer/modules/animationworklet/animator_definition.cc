// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animator_definition.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_animate_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_animator_constructor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_state_callback.h"

namespace blink {

AnimatorDefinition::AnimatorDefinition(V8AnimatorConstructor* constructor,
                                       V8AnimateCallback* animate,
                                       V8StateCallback* state)
    : constructor_(constructor), animate_(animate), state_(state) {
  DCHECK(constructor_);
  DCHECK(animate_);
}

AnimatorDefinition::~AnimatorDefinition() = default;

void AnimatorDefinition::Trace(Visitor* visitor) {
  visitor->Trace(constructor_);
  visitor->Trace(animate_);
  visitor->Trace(state_);
}

}  // namespace blink
