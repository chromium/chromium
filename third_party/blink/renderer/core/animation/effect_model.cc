// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/effect_model.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_keyframe_effect_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {
absl::optional<EffectModel::CompositeOperation>
EffectModel::StringToCompositeOperation(const String& composite_string) {
  DCHECK(composite_string == "replace" || composite_string == "add" ||
         composite_string == "accumulate" || composite_string == "auto");
  if (composite_string == "auto")
    return absl::nullopt;
  if (composite_string == "add")
    return kCompositeAdd;
  if (composite_string == "accumulate")
    return kCompositeAccumulate;
  return kCompositeReplace;
}

String EffectModel::CompositeOperationToString(
    absl::optional<CompositeOperation> composite) {
  if (!composite)
    return "auto";
  switch (composite.value()) {
    case EffectModel::kCompositeAccumulate:
      return "accumulate";
    case EffectModel::kCompositeAdd:
      return "add";
    case EffectModel::kCompositeReplace:
      return "replace";
    default:
      NOTREACHED();
      return "";
  }
}
}  // namespace blink
