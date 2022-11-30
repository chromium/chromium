// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_result.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

XRHitTestSource::XRHitTestSource(uint64_t id, XRSession* xr_session)
    : id_(id), xr_session_(xr_session) {}

uint64_t XRHitTestSource::id() const {
  return id_;
}

void XRHitTestSource::cancel(ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (!xr_session_->RemoveHitTestSource(this)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kCannotCancelHitTestSource);
  }
}

HeapVector<Member<XRHitTestResult>> XRHitTestSource::Results() {
  HeapVector<Member<XRHitTestResult>> results;

  for (const auto& result : last_frame_results_) {
    results.emplace_back(
        MakeGarbageCollected<XRHitTestResult>(xr_session_, *result));
  }

  return results;
}

void XRHitTestSource::Update(
    const Vector<device::mojom::blink::XRHitResultPtr>& hit_test_results) {
  last_frame_results_.clear();

  for (auto& result : hit_test_results) {
    DVLOG(3) << __func__ << ": processing hit test result, position="
             << result->mojo_from_result.position().ToString()
             << ", orientation="
             << result->mojo_from_result.orientation().ToString()
             << ", plane_id=" << result->plane_id;
    last_frame_results_.emplace_back(result->Clone());
  }
}

void XRHitTestSource::Trace(Visitor* visitor) const {
  visitor->Trace(xr_session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
