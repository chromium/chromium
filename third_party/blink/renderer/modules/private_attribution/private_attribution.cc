// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/private_attribution/private_attribution.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

PrivateAttribution::PrivateAttribution() = default;

// static
ScriptPromise<PrivateAttributionEncryptedMatchKey>
PrivateAttribution::getEncryptedMatchKey(ScriptState*,
                                         String report_collector,
                                         PrivateAttributionOptions* options,
                                         ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "This function is not implemented.");
  return EmptyPromise();
}

// static
ScriptPromise<IDLSequence<PrivateAttributionNetwork>>
PrivateAttribution::getHelperNetworks(ScriptState*,
                                      ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "This function is not implemented.");
  return ScriptPromise<IDLSequence<PrivateAttributionNetwork>>();
}

void PrivateAttribution::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
