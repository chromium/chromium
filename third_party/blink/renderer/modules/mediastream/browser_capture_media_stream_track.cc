// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

BrowserCaptureMediaStreamTrack::BrowserCaptureMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    base::OnceClosure callback,
    const String& descriptor_id,
    bool is_clone)
    : BrowserCaptureMediaStreamTrack(execution_context,
                                     component,
                                     component->Source()->GetReadyState(),
                                     std::move(callback),
                                     descriptor_id,
                                     is_clone) {}

BrowserCaptureMediaStreamTrack::BrowserCaptureMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    MediaStreamSource::ReadyState ready_state,
    base::OnceClosure callback,
    const String& descriptor_id,
    bool is_clone)
    : FocusableMediaStreamTrack(execution_context,
                                component,
                                ready_state,
                                std::move(callback),
                                descriptor_id,
                                is_clone) {}

ScriptPromise BrowserCaptureMediaStreamTrack::cropTo(
    ScriptState* script_state,
    const String& crop_id,
    ExceptionState& exception_state) {
  // TODO(crbug.com/1247761): Implement.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kInvalidStateError, "Not implemented."));
  return promise;
}

BrowserCaptureMediaStreamTrack* BrowserCaptureMediaStreamTrack::clone(
    ScriptState* script_state) {
  // Instantiate the clone.
  BrowserCaptureMediaStreamTrack* cloned_track =
      MakeGarbageCollected<BrowserCaptureMediaStreamTrack>(
          ExecutionContext::From(script_state), Component()->Clone(),
          GetReadyState(), base::DoNothing(), descriptor_id(),
          /*is_clone=*/true);

  // Copy state.
  CloneInternal(cloned_track);

  return cloned_track;
}

void BrowserCaptureMediaStreamTrack::CloneInternal(
    BrowserCaptureMediaStreamTrack* cloned_track) {
  // Clone parent classes' state.
  FocusableMediaStreamTrack::CloneInternal(cloned_track);

  // TODO(crbug.com/1247761): Clone cropping-related state.
}

}  // namespace blink
