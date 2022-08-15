// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/focusable_media_stream_track.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"

namespace blink {

FocusableMediaStreamTrack::FocusableMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    base::OnceClosure callback,
    const String& descriptor_id,
    bool is_clone)
    : FocusableMediaStreamTrack(execution_context,
                                component,
                                component->GetReadyState(),
                                std::move(callback),
                                descriptor_id,
                                is_clone) {}

FocusableMediaStreamTrack::FocusableMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    MediaStreamSource::ReadyState ready_state,
    base::OnceClosure callback,
    const String& descriptor_id,
    bool is_clone)
    : MediaStreamTrackImpl(execution_context,
                           component,
                           ready_state,
                           std::move(callback)),
#if !BUILDFLAG(IS_ANDROID)
      is_clone_(is_clone),
#endif
      descriptor_id_(descriptor_id) {
}

#if !BUILDFLAG(IS_ANDROID)
void FocusableMediaStreamTrack::CloseFocusWindowOfOpportunity() {
  promise_settled_ = true;
}
#endif

void FocusableMediaStreamTrack::focus(
    ExecutionContext* execution_context,
    V8CaptureStartFocusBehavior focus_behavior,
    ExceptionState& exception_state) {
#if !BUILDFLAG(IS_ANDROID)
  UserMediaClient* const client =
      UserMediaClient::From(To<LocalDOMWindow>(execution_context));
  if (!client) {
    DLOG(ERROR) << "UserMediaClient missing.";
    return;
  }

  if (is_clone_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Method may not be invoked on clones.");
    return;
  }

  if (focus_called_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Method may only be called once.");
    return;
  }
  focus_called_ = true;

  if (promise_settled_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The microtask on which the Promise was settled has terminated.");
    return;
  }

  client->FocusCapturedSurface(
      descriptor_id_,
      focus_behavior.AsEnum() ==
          V8CaptureStartFocusBehavior::Enum::kFocusCapturedSurface);
#endif
}

FocusableMediaStreamTrack* FocusableMediaStreamTrack::clone(
    ExecutionContext* execution_context) {
  // Instantiate the clone.
  FocusableMediaStreamTrack* cloned_track =
      MakeGarbageCollected<FocusableMediaStreamTrack>(
          execution_context, Component()->Clone(ClonePlatformTrack()),
          GetReadyState(), base::DoNothing(), descriptor_id_,
          /*is_clone=*/true);

  // Copy state.
  FocusableMediaStreamTrack::CloneInternal(cloned_track);

  return cloned_track;
}

void FocusableMediaStreamTrack::CloneInternal(
    FocusableMediaStreamTrack* cloned_track) {
  // Clone parent classes' state.
  MediaStreamTrackImpl::CloneInternal(cloned_track);

  // Clone own state.
#if !BUILDFLAG(IS_ANDROID)
  // Copied for completeness, but should never be read on clones.
  cloned_track->focus_called_ = focus_called_;
  cloned_track->promise_settled_ = promise_settled_;
#endif
}

}  // namespace blink
