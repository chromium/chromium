// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/effect_model.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_keyframe_effect_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {
EffectModel::CompositeOperation EffectModel::EnumToCompositeOperation(
    V8CompositeOperation::Enum composite) {
  switch (composite) {
    case V8CompositeOperation::Enum::kAccumulate:
      return EffectModel::kCompositeAccumulate;
    case V8CompositeOperation::Enum::kAdd:
      return EffectModel::kCompositeAdd;
    case V8CompositeOperation::Enum::kReplace:
      return EffectModel::kCompositeReplace;
  }
}

std::optional<EffectModel::CompositeOperation>
EffectModel::EnumToCompositeOperation(
    V8CompositeOperationOrAuto::Enum composite) {
  switch (composite) {
    case V8CompositeOperationOrAuto::Enum::kAccumulate:
      return EffectModel::kCompositeAccumulate;
    case V8CompositeOperationOrAuto::Enum::kAdd:
      return EffectModel::kCompositeAdd;
    case V8CompositeOperationOrAuto::Enum::kReplace:
      return EffectModel::kCompositeReplace;
    case V8CompositeOperationOrAuto::Enum::kAuto:
      return std::nullopt;
  }
}

V8CompositeOperation::Enum EffectModel::CompositeOperationToEnum(
    CompositeOperation composite) {
  switch (composite) {
    case EffectModel::kCompositeAccumulate:
      return V8CompositeOperation::Enum::kAccumulate;
    case EffectModel::kCompositeAdd:
      return V8CompositeOperation::Enum::kAdd;
    case EffectModel::kCompositeReplace:
      return V8CompositeOperation::Enum::kReplace;
  }
}
}  // namespace blink
