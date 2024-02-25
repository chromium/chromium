// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/crop_target.h"

#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/mediastream/media_devices.h"
#include "third_party/blink/renderer/modules/mediastream/sub_capture_target.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptPromise CropTarget::fromElement(ScriptState* script_state,
                                      Element* element,
                                      ExceptionState& exception_state) {
  return SubCaptureTarget::fromElement(script_state, element, exception_state,
                                       SubCaptureTarget::Type::kCropTarget);
}

CropTarget::CropTarget(String id)
    : SubCaptureTarget(SubCaptureTarget::Type::kCropTarget, std::move(id)) {}

}  // namespace blink
