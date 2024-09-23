// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/apply_constraints_request.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ApplyConstraintsRequest::ApplyConstraintsRequest(
    MediaStreamTrack* track,
    const MediaConstraints& constraints,
    ScriptPromiseResolver<IDLUndefined>* resolver)
    : track_(track), constraints_(constraints), resolver_(resolver) {}

MediaStreamComponent* ApplyConstraintsRequest::Track() const {
  return track_->Component();
}

MediaConstraints ApplyConstraintsRequest::Constraints() const {
  return constraints_;
}

void ApplyConstraintsRequest::RequestSucceeded() {
  track_->SetConstraints(constraints_);
  if (resolver_)
    resolver_->Resolve();
  track_ = nullptr;
}

void ApplyConstraintsRequest::RequestFailed(const String& constraint,
                                            const String& message) {
  if (resolver_) {
    resolver_->Reject(
        MakeGarbageCollected<OverconstrainedError>(constraint, message));
  }
  track_ = nullptr;
}

void ApplyConstraintsRequest::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  visitor->Trace(resolver_);
}

}  // namespace blink
