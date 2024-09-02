// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"

#include <cmath>
#include <optional>

#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_captured_wheel_action.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using ::blink::mojom::blink::CapturedSurfaceControlResult;

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
  NOTREACHED();
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

std::optional<base::UnguessableToken> GetCaptureSessionId(
    MediaStreamTrack* track) {
  if (!track) {
    return std::nullopt;
  }
  MediaStreamComponent* component = track->Component();
  if (!component) {
    return std::nullopt;
  }
  MediaStreamSource* source = component->Source();
  if (!source) {
    return std::nullopt;
  }
  WebPlatformMediaStreamSource* platform_source = source->GetPlatformSource();
  if (!platform_source) {
    return std::nullopt;
  }
  return platform_source->device().serializable_session_id();
}

DOMException* CscResultToDOMException(CapturedSurfaceControlResult result) {
  switch (result) {
    case CapturedSurfaceControlResult::kSuccess:
      return nullptr;
    case CapturedSurfaceControlResult::kUnknownError:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError,
                                                "Unknown error.");
    case CapturedSurfaceControlResult::kNoPermissionError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, "No permission.");
    case CapturedSurfaceControlResult::kCapturerNotFoundError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, "Capturer not found.");
    case CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, "Captured surface not found.");
    case CapturedSurfaceControlResult::kDisallowedForSelfCaptureError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "API not supported for self-capture.");
    case CapturedSurfaceControlResult::kCapturerNotFocusedError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "Capturing application not focused.");
  }
  NOTREACHED_NORETURN();
}

void OnCapturedSurfaceControlResult(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    CapturedSurfaceControlResult result) {
  if (auto* exception = CscResultToDOMException(result)) {
    resolver->Reject(exception);
  } else {
    resolver->Resolve();
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
class CaptureController::WheelEventListener : public NativeEventListener {
 public:
  WheelEventListener(ScriptState* script_state, CaptureController* controller)
      : script_state_(script_state), controller_(controller) {}

  void ListenTo(HTMLElement* element) {
    if (element_) {
      element_->removeEventListener(event_type_names::kWheel, this,
                                    /*use_capture=*/false);
    }
    element_ = element;
    if (element_) {
      element_->addEventListener(event_type_names::kWheel, this);
    }
  }

  void StopListening() { ListenTo(nullptr); }

  // NativeEventListener
  void Invoke(ExecutionContext* context, Event* event) override {
    CHECK(element_);
    CHECK(controller_);
    WheelEvent* wheel_event = DynamicTo<WheelEvent>(event);
    if (!wheel_event || !wheel_event->isTrusted()) {
      return;
    }

    DOMRect* element_rect = element_->GetBoundingClientRect();
    double relative_x =
        static_cast<double>(wheel_event->offsetX()) / element_rect->width();
    double relative_y =
        static_cast<double>(wheel_event->offsetY()) / element_rect->height();

    controller_->SendWheel(relative_x, relative_y, -wheel_event->deltaX(),
                           -wheel_event->deltaY());
  }

  void Trace(Visitor* visitor) const override {
    NativeEventListener::Trace(visitor);
    visitor->Trace(script_state_);
    visitor->Trace(controller_);
    visitor->Trace(element_);
  }

 private:
  Member<ScriptState> script_state_;
  Member<CaptureController> controller_;
  Member<HTMLElement> element_;
};
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

CaptureController::ValidationResult::ValidationResult(DOMExceptionCode code,
                                                      String message)
    : code(code), message(message) {}

CaptureController* CaptureController::Create(ExecutionContext* context) {
  return MakeGarbageCollected<CaptureController>(context);
}

CaptureController::CaptureController(ExecutionContext* context)
    : ExecutionContextClient(context)
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      ,
      media_stream_dispatcher_host_(context)
#endif
{
}

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

ScriptPromise<IDLUndefined> CaptureController::sendWheel(
    ScriptState* script_state,
    CapturedWheelAction* action) {
  DCHECK(IsMainThread());
  CHECK(action);
  CHECK(action->hasX());
  CHECK(action->hasY());
  CHECK(action->hasWheelDeltaX());
  CHECK(action->hasWheelDeltaY());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);

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

  const std::optional<base::UnguessableToken>& session_id =
      GetCaptureSessionId(video_track_);
  if (!session_id.has_value()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kUnknownError,
                                     "Invalid capture");
    return promise;
  }

  GetMediaStreamDispatcherHost()->SendWheel(
      *session_id,
      blink::mojom::blink::CapturedWheelAction::New(
          scaled_coordinates->relative_x, scaled_coordinates->relative_y,
          action->wheelDeltaX(), action->wheelDeltaY()),
      WTF::BindOnce(&OnCapturedSurfaceControlResult, WrapPersistent(resolver)));

  return promise;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

ScriptPromise<IDLUndefined> CaptureController::captureWheel(
    ScriptState* script_state,
    HTMLElement* element) {
  DCHECK(IsMainThread());
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  const auto promise = resolver->Promise();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                   "Unsupported.");
  return promise;
#else
  if (!element) {
    if (wheel_listener_) {
      wheel_listener_->StopListening();
    }
    resolver->Resolve();
    return promise;
  }

  std::optional<base::UnguessableToken> session_id =
      GetCaptureSessionId(video_track_);
  if (!session_id.has_value()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "Invalid capture.");
    return promise;
  }

  ValidationResult validation_result = ValidateCapturedSurfaceControlCall();
  if (validation_result.code != DOMExceptionCode::kNoError) {
    resolver->RejectWithDOMException(validation_result.code,
                                     validation_result.message);
    return promise;
  }

  GetMediaStreamDispatcherHost()->RequestCapturedSurfaceControlPermission(
      *session_id,
      WTF::BindOnce(&CaptureController::OnCaptureWheelPermissionResult,
                    WrapWeakPersistent(this), WrapPersistent(resolver),
                    WrapWeakPersistent(element)));
  return promise;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

Vector<int> CaptureController::getSupportedZoomLevels() {
  const wtf_size_t kSize =
      static_cast<wtf_size_t>(kPresetBrowserZoomFactors.size());
  // If later developers modify `kPresetBrowserZoomFactors` to include many more
  // entries than original intended, they should consider modifying this
  // Web-exposed API to either:
  // * Allow the Web application provide the max levels it wishes to receive.
  // * Do some UA-determined trimming.
  CHECK_LE(kSize, 100u) << "Excessive zoom levels.";
  CHECK_EQ(kMinimumBrowserZoomFactor, kPresetBrowserZoomFactors.front());
  CHECK_EQ(kMaximumBrowserZoomFactor, kPresetBrowserZoomFactors.back());

  Vector<int> result(kSize);
  if (kSize == 0) {
    return result;
  }

  result[0] = static_cast<int>(std::ceil(100 * kPresetBrowserZoomFactors[0]));
  for (wtf_size_t i = 1; i < kSize; ++i) {
    result[i] =
        static_cast<int>(std::floor(100 * kPresetBrowserZoomFactors[i]));
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

ScriptPromise<IDLUndefined> CaptureController::setZoomLevel(
    ScriptState* script_state,
    int zoom_level) {
  DCHECK(IsMainThread());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);

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

  const std::optional<base::UnguessableToken>& session_id =
      GetCaptureSessionId(video_track_);
  if (!session_id.has_value()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kUnknownError,
                                     "Invalid capture");
    return promise;
  }

  GetMediaStreamDispatcherHost()->SetZoomLevel(
      session_id.value(), zoom_level,
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

void CaptureController::OnCaptureWheelPermissionResult(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    HTMLElement* element,
    CapturedSurfaceControlResult result) {
  DCHECK(IsMainThread());
  DOMException* exception = CscResultToDOMException(result);
  if (exception) {
    resolver->Reject(exception);
    return;
  }

  if (!wheel_listener_) {
    wheel_listener_ = MakeGarbageCollected<WheelEventListener>(
        resolver->GetScriptState(), this);
  }
  wheel_listener_->ListenTo(element);
  resolver->Resolve();
}

void CaptureController::SendWheel(double relative_x,
                                  double relative_y,
                                  int32_t wheel_delta_x,
                                  int32_t wheel_delta_y) {
  const std::optional<base::UnguessableToken>& session_id =
      GetCaptureSessionId(video_track_);
  if (!session_id.has_value()) {
    return;
  }

  GetMediaStreamDispatcherHost()->SendWheel(
      *session_id,
      blink::mojom::blink::CapturedWheelAction::New(
          relative_x, relative_y, wheel_delta_x, wheel_delta_y),
      WTF::BindOnce([](CapturedSurfaceControlResult) {}));
}

mojom::blink::MediaStreamDispatcherHost*
CaptureController::GetMediaStreamDispatcherHost() {
  DCHECK(IsMainThread());
  CHECK(GetExecutionContext());
  if (!media_stream_dispatcher_host_.is_bound()) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        media_stream_dispatcher_host_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(
                TaskType::kInternalMediaRealTime)));
  }

  return media_stream_dispatcher_host_.get();
}

void CaptureController::SetMediaStreamDispatcherHostForTesting(
    mojo::PendingRemote<mojom::blink::MediaStreamDispatcherHost> host) {
  media_stream_dispatcher_host_.Bind(
      std::move(host),
      GetExecutionContext()->GetTaskRunner(TaskType::kInternalMediaRealTime));
}
#endif

void CaptureController::Trace(Visitor* visitor) const {
  visitor->Trace(video_track_);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  visitor->Trace(wheel_listener_);
  visitor->Trace(media_stream_dispatcher_host_);
#endif
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
