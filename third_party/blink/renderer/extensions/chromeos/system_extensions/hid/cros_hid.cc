// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/hid/cros_hid.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_device_request_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

void CrosHID::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

ScriptPromise CrosHID::requestDevice(ScriptState* script_state,
                                     const HIDDeviceRequestOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  // TODO(b/214330353): Implement this method via a Mojo pipe.
  resolver->Reject("Not implemented");
  return resolver->Promise();
}

}  // namespace blink
