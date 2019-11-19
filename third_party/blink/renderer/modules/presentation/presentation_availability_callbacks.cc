// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_availability_callbacks.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability.h"
#include "third_party/blink/renderer/modules/presentation/presentation_error.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

DOMException* CreateAvailabilityNotSupportedError() {
  static const WebString& not_supported_error = blink::WebString::FromUTF8(
      "getAvailability() isn't supported at the moment. It can be due to "
      "a permanent or temporary system limitation. It is recommended to "
      "try to blindly start a presentation in that case.");
  return MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, not_supported_error);
}

}  // namespace

PresentationAvailabilityCallbacks::PresentationAvailabilityCallbacks(
    PresentationAvailabilityProperty* resolver,
    const Vector<KURL>& urls)
    : resolver_(resolver), urls_(urls) {}

PresentationAvailabilityCallbacks::~PresentationAvailabilityCallbacks() =
    default;

void PresentationAvailabilityCallbacks::Resolve(bool value) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver_->Resolve(
      PresentationAvailability::Take(resolver_.Get(), urls_, value));
}

void PresentationAvailabilityCallbacks::RejectAvailabilityNotSupported() {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver_->Reject(CreateAvailabilityNotSupportedError());
}

void PresentationAvailabilityCallbacks::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
}

}  // namespace blink
