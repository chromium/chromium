// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"

#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_captured_wheel_action.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
struct ScaledCoordinates {
  ScaledCoordinates(double relative_x, double relative_y)
      : relative_x(relative_x), relative_y(relative_y) {
    CHECK(0.0 <= relative_x && relative_x < 1.0);
    CHECK(0.0 <= relative_y && relative_y < 1.0);
  }

  const double relative_x;
  const double relative_y;
};

// Attempt to scale the coordinates to relative coordinates based on the last
// frame emitted for the given track.
base::expected<ScaledCoordinates, String> ScaleCoordinates(
    MediaStreamTrack* track,
    CapturedWheelAction* action) {
  CHECK(track);  // Validated by ValidateCapturedSurfaceControlCall().

  MediaStreamComponent* const component = track->Component();
  if (!component) {
    return base::unexpected("Unexpected error - no component.");
  }

  MediaStreamVideoTrack* const video_track =
      MediaStreamVideoTrack::From(component);
  if (!video_track) {
    return base::unexpected("Unexpected error - no video track.");
  }

  // Determine the size of the last video frame observed by the app for this
  // capture session.
  const gfx::Size last_frame_size = video_track->GetVideoSize();

  // Validate (x, y) prior to scaling.
  if (last_frame_size.width() <= 0 || last_frame_size.height() <= 0) {
    return base::unexpected("No frames observed yet.");
  }
  if (action->x() < 0 || action->x() >= last_frame_size.width() ||
      action->y() < 0 || action->y() >= last_frame_size.height()) {
    return base::unexpected("Coordinates out of bounds.");
  }

  // Scale (x, y) to reflect their position relative to the video size.
  // This allows the browser process to scale these coordinates to
  // the coordinate space of the captured surface, which is unknown
  // to the capturer.
  const double relative_x =
      static_cast<double>(action->x()) / last_frame_size.width();
  const double relative_y =
      static_cast<double>(action->y()) / last_frame_size.height();
  return ScaledCoordinates(relative_x, relative_y);
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

void OnCapturedSurfaceControlResult(ScriptPromiseResolver* resolver,
                                    bool success,
                                    const String& error) {
  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, error));
  }
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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
  CHECK(action->hasX());
  CHECK(action->hasY());
  CHECK(action->hasWheelDeltaX());
  CHECK(action->hasWheelDeltaY());

  ScriptPromiseResolver* const resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  const ScriptPromise promise = resolver->Promise();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, "Unsupported."));
  return promise;
#else
  std::pair<bool, DOMException*> validation_result =
      ValidateCapturedSurfaceControlCall();
  if (!validation_result.first) {
    resolver->Reject(validation_result.second);
    return promise;
  }

  const base::expected<ScaledCoordinates, String> scaled_coordinates =
      ScaleCoordinates(video_track_, action);
  if (!scaled_coordinates.has_value()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, scaled_coordinates.error()));
    return promise;
  }

  video_track_->SendWheel(
      scaled_coordinates->relative_x, scaled_coordinates->relative_y,
      action->wheelDeltaX(), action->wheelDeltaY(),
      WTF::BindOnce(&OnCapturedSurfaceControlResult, WrapPersistent(resolver)));

  return promise;
#endif  // !BUILDFLAG(IS_ANDROID)
}

Vector<int> CaptureController::getSupportedZoomLevels() {
  const wtf_size_t kSize = static_cast<wtf_size_t>(kPresetZoomFactors.size());
  // If later developers modify `kPresetZoomFactors` to include many more
  // entries than original intended, they should consider modifying this
  // Web-exposed API to either:
  // * Allow the Web application provide the max levels it wishes to receive.
  // * Do some UA-determined trimming.
  CHECK_LE(kSize, 100u) << "Excessive zoom levels.";
  CHECK_EQ(kMinimumPageZoomFactor, kPresetZoomFactors.front());
  CHECK_EQ(kMaximumPageZoomFactor, kPresetZoomFactors.back());

  Vector<int> result(kSize);
  if (kSize == 0) {
    return result;
  }

  result[0] = static_cast<int>(std::ceil(100 * kPresetZoomFactors[0]));
  for (wtf_size_t i = 1; i < kSize; ++i) {
    result[i] = static_cast<int>(std::floor(100 * kPresetZoomFactors[i]));
    CHECK_LT(result[i - 1], result[i]) << "Must be monotonically increasing.";
  }

  return result;
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
  std::pair<bool, DOMException*> validation_result =
      ValidateCapturedSurfaceControlCall();
  if (!validation_result.first) {
    resolver->Reject(validation_result.second);
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

ScriptPromise CaptureController::setZoomLevel(ScriptState* script_state,
                                              int zoom_level) {
  DCHECK(IsMainThread());

  ScriptPromiseResolver* const resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  const ScriptPromise promise = resolver->Promise();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, "Unsupported."));
  return promise;
#else
  std::pair<bool, DOMException*> validation_result =
      ValidateCapturedSurfaceControlCall();
  if (!validation_result.first) {
    resolver->Reject(validation_result.second);
    return promise;
  }

  if (!getSupportedZoomLevels().Contains(zoom_level)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Only values returned by getSupportedZoomLevels() are valid."));
    return promise;
  }

  video_track_->SetZoomLevel(
      zoom_level,
      WTF::BindOnce(&OnCapturedSurfaceControlResult, WrapPersistent(resolver)));
  return promise;
#endif  // !BUILDFLAG(IS_ANDROID)
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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

std::pair<bool, DOMException*>
CaptureController::ValidateCapturedSurfaceControlCall() const {
  if (!is_bound_) {
    return std::make_pair(false, MakeGarbageCollected<DOMException>(
                                     DOMExceptionCode::kInvalidStateError,
                                     "getDisplayMedia() not called yet."));
  }

  if (!video_track_) {
    return std::make_pair(false, MakeGarbageCollected<DOMException>(
                                     DOMExceptionCode::kInvalidStateError,
                                     "Capture-session not started."));
  }

  if (video_track_->readyState() == "ended") {
    return std::make_pair(
        false, MakeGarbageCollected<DOMException>(
                   DOMExceptionCode::kInvalidStateError, "Video track ended."));
  }

  if (!IsCaptureType(video_track_, {SurfaceType::BROWSER})) {
    return std::make_pair(false, MakeGarbageCollected<DOMException>(
                                     DOMExceptionCode::kNotSupportedError,
                                     "Action only supported for tab-capture."));
  }
  return std::make_pair(true, nullptr);
}

}  // namespace blink
