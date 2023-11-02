// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_animation_options.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"

namespace blink {

WorkletAnimationOptions::WorkletAnimationOptions(
    scoped_refptr<SerializedScriptValue> data)
    : data_(data) {}

std::unique_ptr<cc::AnimationOptions> WorkletAnimationOptions::Clone() const {
  return std::make_unique<WorkletAnimationOptions>(data_);
}

WorkletAnimationOptions::~WorkletAnimationOptions() {}

}  // namespace blink
