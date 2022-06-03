// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/apply_constraints_request.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

ApplyConstraintsRequest::ApplyConstraintsRequest(
    MediaStreamComponent* component,
    const MediaConstraints& constraints,
    ScriptPromiseResolver* resolver)
    : component_(component), constraints_(constraints), resolver_(resolver) {}

MediaStreamComponent* ApplyConstraintsRequest::Track() const {
  return component_;
}

MediaConstraints ApplyConstraintsRequest::Constraints() const {
  return constraints_;
}

void ApplyConstraintsRequest::RequestSucceeded() {
  component_->SetConstraints(constraints_);
  if (resolver_)
    resolver_->Resolve();
  component_ = nullptr;
}

void ApplyConstraintsRequest::RequestFailed(const String& constraint,
                                            const String& message) {
  if (resolver_) {
    resolver_->Reject(
        MakeGarbageCollected<OverconstrainedError>(constraint, message));
  }
  component_ = nullptr;
}

void ApplyConstraintsRequest::Trace(Visitor* visitor) const {
  visitor->Trace(component_);
  visitor->Trace(resolver_);
}

}  // namespace blink
