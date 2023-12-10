// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"

#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_captured_wheel_action.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

using SurfaceType = media::mojom::DisplayCaptureSurfaceType;

bool IsCaptureType(const MediaStreamTrack* track,
                   const std::vector<SurfaceType>& types) {
  DCHECK(track);

  const MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::From(track->Component());
  if (!video_track) {
    return false;
  }

  MediaStreamTrackPlatform::Settings settings;
  video_track->GetSettings(settings);
  const absl::optional<SurfaceType> display_surface = settings.display_surface;
  return base::ranges::any_of(
      types, [display_surface](SurfaceType t) { return t == display_surface; });
}

#if !BUILDFLAG(IS_ANDROID)
bool IsValid(CapturedWheelAction* action) {
  CHECK(action->hasX());
  CHECK(action->hasY());
  CHECK(action->hasWheelDeltaX());
  CHECK(action->hasWheelDeltaY());
  return action->x() >= 0 && action->y() >= 0;
}

bool ShouldFocusCapturedSurface(V8CaptureStartFocusBehavior focus_behavior) {
  switch (focus_behavior.AsEnum()) {
    case V8CaptureStartFocusBehavior::Enum::kFocusCapturedSurface:
      return true;
    case V8CaptureStartFocusBehavior::Enum::kFocusCapturingApplication:
    case V8CaptureStartFocusBehavior::Enum::kNoFocusChange:
      return false;
  }
  NOTREACHED_NORETURN();
}
#endif  // !BUILDFLAG(IS_ANDROID)

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

  if (!IsCaptureType(video_track_,
                     {SurfaceType::BROWSER, SurfaceType::WINDOW})) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The captured display surface must be either a tab or a window.");
    return;
  }

  focus_behavior_ = focus_behavior;
  FinalizeFocusDecision();
}

ScriptPromise CaptureController::sendWheel(ScriptState* script_state,
                                           CapturedWheelAction* action) {
  DCHECK(IsMainThread());
  CHECK(action);

  ScriptPromiseResolver* const resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  const ScriptPromise promise = resolver->Promise();

#if BUILDFLAG(IS_ANDROID)
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, "Unsupported."));
  return promise;
#else

  if (!is_bound_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "getDisplayMedia() not called yet."));
    return promise;
  }

  if (!video_track_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Capture-session not started."));
    return promise;
  }

  if (video_track_->readyState() == "ended") {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Video track ended."));
    return promise;
  }

  if (!IsCaptureType(video_track_, {SurfaceType::BROWSER})) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Action only supported for tab-capture."));
    return promise;
  }

  if (!IsValid(action)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Invalid action."));
    return promise;
  }

  base::OnceCallback<void(bool, const String&)> callback = WTF::BindOnce(
      [](ScriptPromiseResolver* resolver, bool success, const String& error) {
        if (success) {
          resolver->Resolve();
        } else {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kUnknownError, error));
        }
      },
      WrapPersistent(resolver));

  video_track_->SendWheel(action, std::move(callback));

  return promise;
#endif  // !BUILDFLAG(IS_ANDROID)
}

int CaptureController::getMinZoomLevel() {
  // We expect `100 * kMinimumPageZoomFactor` to be an integer. But if it's not,
  // over-reporting the minimum is preferable, as it would mean the application
  // still asks to set zoom levels which aren't below the minimum.
  return static_cast<int>(std::ceil(100 * kMinimumPageZoomFactor));
}

int CaptureController::getMaxZoomLevel() {
  // We expect `100 * kMaximumPageZoomFactor` to be an integer. But if it's not,
  // under-reporting the maximum is preferable, as it would mean the application
  // still asks to set zoom levels which aren't above the maximum.
  return static_cast<int>(std::floor(100 * kMaximumPageZoomFactor));
}

ScriptPromise CaptureController::getZoomLevel(ScriptState* script_state) {
  DCHECK(IsMainThread());

  ScriptPromiseResolver* const resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  const ScriptPromise promise = resolver->Promise();

#if BUILDFLAG(IS_ANDROID)
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, "Unsupported."));
  return promise;
#else

  if (!is_bound_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "getDisplayMedia() not called yet."));
    return promise;
  }

  if (!video_track_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Capture-session not started."));
    return promise;
  }

  if (video_track_->readyState() == "ended") {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Video track ended."));
    return promise;
  }

  if (!IsCaptureType(video_track_, {SurfaceType::BROWSER})) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Action only supported for tab-capture."));
    return promise;
  }

  base::OnceCallback<void(absl::optional<int>, const String&)> callback =
      WTF::BindOnce(
          [](ScriptPromiseResolver* resolver, absl::optional<int> zoom_level,
             const String& error) {
            if (zoom_level) {
              resolver->Resolve(*zoom_level);
            } else {
              resolver->Reject(MakeGarbageCollected<DOMException>(
                  DOMExceptionCode::kUnknownError, error));
            }
          },
          WrapPersistent(resolver));

  video_track_->GetZoomLevel(std::move(callback));

  return promise;
#endif  // !BUILDFLAG(IS_ANDROID)
}

ScriptPromise CaptureController::setZoomLevel(int zoom_level) {
  // TODO(crbug.com/1466247): Implement.
  NOTIMPLEMENTED();
  return ScriptPromise();
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

  if (!video_track_ || !IsCaptureType(video_track_, {SurfaceType::BROWSER,
                                                     SurfaceType::WINDOW})) {
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
  client->FocusCapturedSurface(
      String(descriptor_id_),
      ShouldFocusCapturedSurface(focus_behavior_.value()));
#endif
}

void CaptureController::Trace(Visitor* visitor) const {
  visitor->Trace(video_track_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
