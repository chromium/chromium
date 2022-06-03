// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_TRANSIENT_INPUT_HIT_TEST_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_TRANSIENT_INPUT_HIT_TEST_RESULT_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"

namespace blink {

class XRInputSource;
class XRHitTestResult;

class XRTransientInputHitTestResult : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRTransientInputHitTestResult(
      XRInputSource* input_source,
      const Vector<device::mojom::blink::XRHitResultPtr>& results);

  XRInputSource* inputSource();

  HeapVector<Member<XRHitTestResult>> results();

  void Trace(Visitor* visitor) const override;

 private:
  Member<XRInputSource> input_source_;
  HeapVector<Member<XRHitTestResult>> results_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_TRANSIENT_INPUT_HIT_TEST_RESULT_H_
