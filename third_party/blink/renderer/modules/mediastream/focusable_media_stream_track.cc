// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/focusable_media_stream_track.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_controller.h"

namespace blink {

FocusableMediaStreamTrack::FocusableMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    base::OnceClosure callback,
    const String& descriptor_id)
    : MediaStreamTrack(execution_context, component, std::move(callback)),
      descriptor_id_(descriptor_id) {}

MediaStreamTrack* FocusableMediaStreamTrack::clone(ScriptState* script_state) {
  // Clones do not expose focus(). They are intentionally of the parent type.
  return MediaStreamTrack::clone(script_state);
}

void FocusableMediaStreamTrack::focus(
    ExecutionContext* execution_context,
    V8CaptureStartFocusBehavior focus_behavior,
    ExceptionState& exception_state) {
#if !defined(OS_ANDROID)
  UserMediaController* const controller =
      UserMediaController::From(To<LocalDOMWindow>(execution_context));
  if (!controller) {
    DLOG(ERROR) << "UserMediaController missing.";
    return;
  }

  UserMediaClient* const client = controller->Client();
  if (!client) {
    DLOG(ERROR) << "UserMediaClient missing.";
    return;
  }

  if (focus_called_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Method may only be called once.");
    return;
  }
  focus_called_ = true;

  client->FocusCapturedSurface(
      descriptor_id_,
      focus_behavior.AsEnum() ==
          V8CaptureStartFocusBehavior::Enum::kFocusCapturedSurface);
#endif
}

}  // namespace blink
