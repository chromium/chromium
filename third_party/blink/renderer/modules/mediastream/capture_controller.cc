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
#include "third_party/blink/renderer/core/dom/events/event.h"
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
  const std::optional<SurfaceType> display_surface = settings.display_surface;
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

void OnCapturedSurfaceControlResult(
    ScriptPromiseResolverTyped<IDLUndefined>* resolver,
    DOMException* exception) {
  if (exception) {
    resolver->Reject(exception);
  } else {
    resolver->Resolve();
  }
}

std::optional<int> GetInitialZoomLevel(MediaStreamTrack* video_track) {
  const MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(
          video_track->Component()->Source());
  if (!native_source) {
    return std::nullopt;
  }

  const media::mojom::DisplayMediaInformationPtr& display_media_info =
      native_source->device().display_media_info;
  if (!display_media_info) {
    return std::nullopt;
  }

  return display_media_info->initial_zoom_level;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

CaptureController::ValidationResult::ValidationResult(DOMExceptionCode code,
                                                      String message)
    : code(code), message(message) {}

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

ScriptPromiseTyped<IDLUndefined> CaptureController::sendWheel(
    ScriptState* script_state,
    CapturedWheelAction* action) {
  DCHECK(IsMainThread());
  CHECK(action);
  CHECK(action->hasX());
  CHECK(action->hasY());
  CHECK(action->hasWheelDeltaX());
  CHECK(action->hasWheelDeltaY());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolverTyped<IDLUndefined>>(
          script_state);

  const auto promise = resolver->Promise();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                   "Unsupported.");
  return promise;
#else
  ValidationResult validation_result = ValidateCapturedSurfaceControlCall();
  if (validation_result.code != DOMExceptionCode::kNoError) {
    resolver->RejectWithDOMException(validation_result.code,
                                     validation_result.message);
    return promise;
  }

  const base::expected<ScaledCoordinates, String> scaled_coordinates =
      ScaleCoordinates(video_track_, action);
  if (!scaled_coordinates.has_value()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     scaled_coordinates.error());
    return promise;
  }

  video_track_->SendWheel(
      scaled_coordinates->relative_x, scaled_coordinates->relative_y,
      action->wheelDeltaX(), action->wheelDeltaY(),
      WTF::BindOnce(&OnCapturedSurfaceControlResult, WrapPersistent(resolver)));

  return promise;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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

int CaptureController::getZoomLevel(ExceptionState& exception_state) {
  DCHECK(IsMainThread());

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Unsupported.");
  return 100;
#else
  ValidationResult validation_result = ValidateCapturedSurfaceControlCall();
  if (validation_result.code != DOMExceptionCode::kNoError) {
    exception_state.ThrowDOMException(validation_result.code,
                                      validation_result.message);
    return 100;
  }

  if (!zoom_level_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The zoom level is not yet known.");
    return 100;
  }

  return *zoom_level_;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

ScriptPromiseTyped<IDLUndefined> CaptureController::setZoomLevel(
    ScriptState* script_state,
    int zoom_level) {
  DCHECK(IsMainThread());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolverTyped<IDLUndefined>>(
          script_state);

  const auto promise = resolver->Promise();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                   "Unsupported.");
  return promise;
#else
  ValidationResult validation_result = ValidateCapturedSurfaceControlCall();
  if (validation_result.code != DOMExceptionCode::kNoError) {
    resolver->RejectWithDOMException(validation_result.code,
                                     validation_result.message);
    return promise;
  }

  if (!getSupportedZoomLevels().Contains(zoom_level)) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Only values returned by getSupportedZoomLevels() are valid.");
    return promise;
  }

  video_track_->SetZoomLevel(
      zoom_level,
      WTF::BindOnce(&OnCapturedSurfaceControlResult, WrapPersistent(resolver)));
  return promise;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

void CaptureController::SetVideoTrack(MediaStreamTrack* video_track,
                                      std::string descriptor_id) {
  DCHECK(IsMainThread());
  DCHECK(video_track);
  DCHECK(!video_track_);
  DCHECK(!descriptor_id.empty());
  DCHECK(descriptor_id_.empty());

  video_track_ = video_track;
  // The CaptureController-Source mapping cannot change after having been set
  // up, and the observer remains until either object is garbage collected. No
  // explicit deregistration of the observer is necessary.
  video_track_->Component()->AddSourceObserver(this);
  descriptor_id_ = std::move(descriptor_id);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  zoom_level_ = GetInitialZoomLevel(video_track_);
#endif
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void CaptureController::SourceChangedZoomLevel(int zoom_level) {
  DCHECK(IsMainThread());

  if (zoom_level_ == zoom_level) {
    return;
  }

  zoom_level_ = zoom_level;

  if (!video_track_ || video_track_->Ended()) {
    return;
  }

  DispatchEvent(*Event::Create(event_type_names::kCapturedzoomlevelchange));
}
#endif

void CaptureController::Trace(Visitor* visitor) const {
  visitor->Trace(video_track_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

CaptureController::ValidationResult
CaptureController::ValidateCapturedSurfaceControlCall() const {
  if (!is_bound_) {
    return ValidationResult(DOMExceptionCode::kInvalidStateError,
                            "getDisplayMedia() not called yet.");
  }

  if (!video_track_) {
    return ValidationResult(DOMExceptionCode::kInvalidStateError,
                            "Capture-session not started.");
  }

  if (video_track_->readyState() == "ended") {
    return ValidationResult(DOMExceptionCode::kInvalidStateError,
                            "Video track ended.");
  }

  if (!IsCaptureType(video_track_, {SurfaceType::BROWSER})) {
    return ValidationResult(DOMExceptionCode::kNotSupportedError,
                            "Action only supported for tab-capture.");
  }
  return ValidationResult(DOMExceptionCode::kNoError, "");
}

}  // namespace blink
