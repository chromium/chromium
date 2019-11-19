// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/apply_constraints_request.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"

namespace blink {

ApplyConstraintsRequest* ApplyConstraintsRequest::CreateForTesting(
    const WebMediaStreamTrack& track,
    const WebMediaConstraints& constraints) {
  return MakeGarbageCollected<ApplyConstraintsRequest>(track, constraints,
                                                       nullptr);
}

ApplyConstraintsRequest::ApplyConstraintsRequest(
    const WebMediaStreamTrack& track,
    const WebMediaConstraints& constraints,
    ScriptPromiseResolver* resolver)
    : track_(track), constraints_(constraints), resolver_(resolver) {}

WebMediaStreamTrack ApplyConstraintsRequest::Track() const {
  return track_;
}

WebMediaConstraints ApplyConstraintsRequest::Constraints() const {
  return constraints_;
}

void ApplyConstraintsRequest::RequestSucceeded() {
  track_.SetConstraints(constraints_);
  if (resolver_)
    resolver_->Resolve();
  track_.Reset();
}

void ApplyConstraintsRequest::RequestFailed(const String& constraint,
                                            const String& message) {
  if (resolver_) {
    resolver_->Reject(OverconstrainedError::Create(constraint, message));
  }
  track_.Reset();
}

void ApplyConstraintsRequest::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
}

}  // namespace blink
