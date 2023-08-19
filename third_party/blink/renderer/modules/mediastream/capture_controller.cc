// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

bool IsTabOrWindowCapture(const MediaStreamTrack* track) {
  DCHECK(track);

  const MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::From(track->Component());
  if (!video_track) {
    return false;
  }

  MediaStreamTrackPlatform::Settings settings;
  video_track->GetSettings(settings);
  return (settings.display_surface ==
              media::mojom::DisplayCaptureSurfaceType::BROWSER ||
          settings.display_surface ==
              media::mojom::DisplayCaptureSurfaceType::WINDOW);
}

}  // namespace

CaptureController* CaptureController::Create(ExecutionContext* context) {
  return MakeGarbageCollected<CaptureController>(context);
}

CaptureController::CaptureController(ExecutionContext* context)
    : ExecutionContextClient(context) {}

void CaptureController::setFocusBehavior(
    V8CaptureStartFocusBehavior focus_behavior,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!GetExecutionContext()) {
    return;
  }

  if (focus_decision_finalized_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The window of opportunity for focus-decision is closed.");
    return;
  }

  if (!video_track_) {
    focus_behavior_ = focus_behavior;
    return;
  }

  if (video_track_->readyState() != "live") {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The video track must be live.");
    return;
  }

  if (!IsTabOrWindowCapture(video_track_)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The captured display surface must be either a tab or a window.");
    return;
  }

  focus_behavior_ = focus_behavior;
  FinalizeFocusDecision();
}

void CaptureController::SetVideoTrack(MediaStreamTrack* video_track,
                                      std::string descriptor_id) {
  DCHECK(IsMainThread());
  DCHECK(video_track);
  DCHECK(!video_track_);
  DCHECK(!descriptor_id.empty());
  DCHECK(descriptor_id_.empty());

  video_track_ = video_track;
  descriptor_id_ = std::move(descriptor_id);
}

const AtomicString& CaptureController::InterfaceName() const {
  return event_target_names::kCaptureController;
}

ExecutionContext* CaptureController::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void CaptureController::FinalizeFocusDecision() {
  DCHECK(IsMainThread());

  if (focus_decision_finalized_) {
    return;
  }

  focus_decision_finalized_ = true;

  if (!video_track_ || !IsTabOrWindowCapture(video_track_)) {
    return;
  }

  UserMediaClient* client = UserMediaClient::From(DomWindow());
  if (!client) {
    return;
  }

  if (!focus_behavior_) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  const bool focus = focus_behavior_->AsEnum() ==
                     V8CaptureStartFocusBehavior::Enum::kFocusCapturedSurface;
  client->FocusCapturedSurface(String(descriptor_id_), focus);
#endif
}

void CaptureController::Trace(Visitor* visitor) const {
  visitor->Trace(video_track_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}
}  // namespace blink
