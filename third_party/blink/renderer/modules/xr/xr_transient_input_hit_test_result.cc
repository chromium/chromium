// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_result.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_result.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"

namespace blink {

XRTransientInputHitTestResult::XRTransientInputHitTestResult(
    XRInputSource* input_source,
    const Vector<device::mojom::blink::XRHitResultPtr>& results)
    : input_source_(input_source) {
  FrozenArray<XRHitTestResult>::VectorType result_vec;
  for (const auto& result : results) {
    result_vec.push_back(MakeGarbageCollected<XRHitTestResult>(
        input_source->session(), *result));
  }
  results_ =
      MakeGarbageCollected<FrozenArray<XRHitTestResult>>(std::move(result_vec));
}

XRInputSource* XRTransientInputHitTestResult::inputSource() {
  return input_source_.Get();
}

const FrozenArray<XRHitTestResult>& XRTransientInputHitTestResult::results()
    const {
  return *results_.Get();
}

void XRTransientInputHitTestResult::Trace(Visitor* visitor) const {
  visitor->Trace(input_source_);
  visitor->Trace(results_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
