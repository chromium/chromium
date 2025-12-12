// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_devices.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "media/base/media_permission.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom-blink.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver_with_tracker.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_output_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_display_media_stream_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_device_kind.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_supported_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_mediatrackconstraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_domexception_overconstrainederror.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_user_media_stream_constraints.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/input_device_info.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/navigator_media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/restriction_target.h"
#include "third_party/blink/renderer/modules/mediastream/scoped_media_stream_tracer.h"
#include "third_party/blink/renderer/modules/mediastream/sub_capture_target.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

BASE_FEATURE(kEnumerateDevicesRequestAudioCapabilities,
#if BUILDFLAG(IS_MAC)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kDeviceChangeRequiresPermission, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

template <typename IDLResolvedType>
class PromiseResolverCallbacks final : public UserMediaRequest::Callbacks {
 public:
  PromiseResolverCallbacks(
      UserMediaRequestType media_type,
      ScriptPromiseResolverWithTracker<UserMediaRequestResult, IDLResolvedType>*
          resolver,
      base::OnceCallback<void(const String&, CaptureController*)>
          on_success_follow_up,
      std::unique_ptr<ScopedMediaStreamTracer> tracer)
      : media_type_(media_type),
        resolver_(resolver),
        on_success_follow_up_(std::move(on_success_follow_up)),
        tracer_(std::move(tracer)) {}
  ~PromiseResolverCallbacks() override = default;

  void OnSuccess(const MediaStreamVector& streams,
                 CaptureController* capture_controller) override {
    OnSuccessImpl<IDLResolvedType>(streams, capture_controller);
    if (tracer_) {
      tracer_->End();
    }
  }

  template <typename T>
  void OnSuccessImpl(const MediaStreamVector&, CaptureController*);

  void OnError(ScriptWrappable* callback_this_value,
               const V8MediaStreamError* error,
               CaptureController* capture_controller,
               UserMediaRequestResult result) override {
    if (capture_controller) {
      capture_controller->FinalizeFocusDecision();
    }
    resolver_->template Reject<V8MediaStreamError>(error, result);
    if (tracer_) {
      tracer_->End();
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    UserMediaRequest::Callbacks::Trace(visitor);
  }

 private:
  const UserMediaRequestType media_type_;

  Member<
      ScriptPromiseResolverWithTracker<UserMediaRequestResult, IDLResolvedType>>
      resolver_;
  base::OnceCallback<void(const String&, CaptureController*)>
      on_success_follow_up_;
  std::unique_ptr<ScopedMediaStreamTracer> tracer_;
};

template <>
template <>
void PromiseResolverCallbacks<MediaStream>::OnSuccessImpl<MediaStream>(
    const MediaStreamVector& streams,
    CaptureController* capture_controller) {
  DCHECK_EQ(streams.size(), 1u);
  MediaStream* stream = streams[0];

  if (on_success_follow_up_) {
    // Only getDisplayMedia() calls set |on_success_follow_up_|.
    // Successful invocations of getDisplayMedia() always have exactly
    // one video track, except for the case when the permission
    // `DISPLAY_MEDIA_SYSTEM_AUDIO` is set, which will lead to 0 video track.
    //
    // Extension API calls that are followed by a getUserMedia() call with
    // chromeMediaSourceId are treated liked getDisplayMedia() calls.
    MediaStreamTrackVector video_tracks = stream->getVideoTracks();
    if (capture_controller && video_tracks.size() > 0) {
      capture_controller->SetVideoTrack(video_tracks[0], stream->id().Utf8());
    }
  }

  // Resolve Promise<MediaStream> on a microtask.
  resolver_->Resolve(stream);

  // Enqueue the follow-up microtask, if any is intended.
  if (on_success_follow_up_) {
    std::move(on_success_follow_up_).Run(stream->id(), capture_controller);
  }
}

template <>
template <>
void PromiseResolverCallbacks<IDLSequence<MediaStream>>::OnSuccessImpl<
    IDLSequence<MediaStream>>(const MediaStreamVector& streams,
                              CaptureController* capture_controller) {
  DCHECK(!streams.empty());
  DCHECK_EQ(UserMediaRequestType::kAllScreensMedia, media_type_);
  resolver_->Resolve(streams);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DisplayCapturePolicyResult {
  kDisallowed = 0,
  kAllowed = 1,
  kMaxValue = kAllowed
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProduceTargetFunctionResult {
  kPromiseProduced = 0,
  kGenericError = 1,
  kInvalidContext = 2,
  kDuplicateCallBeforePromiseResolution = 3,
  kDuplicateCallAfterPromiseResolution = 4,
  kElementAndMediaDevicesNotInSameExecutionContext = 5,
  kMaxValue = kElementAndMediaDevicesNotInSameExecutionContext
};

void RecordUma(SubCaptureTarget::Type type,
               ProduceTargetFunctionResult result) {
  if (type == SubCaptureTarget::Type::kCropTarget) {
    base::UmaHistogramEnumeration(
        "Media.RegionCapture.ProduceCropTarget.Function.Result", result);
  } else if (type == SubCaptureTarget::Type::kRestrictionTarget) {
    base::UmaHistogramEnumeration(
        "Media.ElementCapture.ProduceTarget.Function.Result", result);
  } else {
    NOTREACHED();
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProduceTargetPromiseResult {
  kPromiseResolved = 0,
  kPromiseRejected = 1,
  kMaxValue = kPromiseRejected
};

void RecordUma(SubCaptureTarget::Type type, ProduceTargetPromiseResult result) {
  if (type == SubCaptureTarget::Type::kCropTarget) {
    base::UmaHistogramEnumeration(
        "Media.RegionCapture.ProduceCropTarget.Promise.Result", result);
  } else if (type == SubCaptureTarget::Type::kRestrictionTarget) {
    base::UmaHistogramEnumeration(
        "Media.ElementCapture.ProduceTarget.Promise.Result", result);
  } else {
    NOTREACHED();
  }
}

// When `blink::features::kGetDisplayMediaRequiresUserActivation` is enabled,
// calls to `getDisplayMedia()` will require a transient user activation. This
// can be bypassed with the `ScreenCaptureWithoutGestureAllowedForOrigins`
// policy though.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetDisplayMediaTransientActivation {
  kPresent = 0,
  kMissing = 1,
  kMissingButFeatureDisabled = 2,
  kMissingButPolicyOverrides = 3,
  kMaxValue = kMissingButPolicyOverrides
};

void RecordUma(GetDisplayMediaTransientActivation activation) {
  base::UmaHistogramEnumeration(
      "Media.GetDisplayMedia.RequiresUserActivationResult", activation);
}

bool TransientActivationRequirementSatisfied(LocalDOMWindow* window) {
  DCHECK(window);

  LocalFrame* const frame = window->GetFrame();
  if (!frame) {
    return false;  // Err on the side of caution. Intentionally neglect UMA.
  }

  const Settings* const settings = frame->GetSettings();
  if (!settings) {
    return false;  // Err on the side of caution. Intentionally neglect UMA.
  }

  if (LocalFrame::HasTransientUserActivation(frame) ||
      (RuntimeEnabledFeatures::
           CapabilityDelegationDisplayCaptureRequestEnabled() &&
       window->IsDisplayCaptureRequestTokenActive())) {
    RecordUma(GetDisplayMediaTransientActivation::kPresent);
    return true;
  }

  if (!RuntimeEnabledFeatures::GetDisplayMediaRequiresUserActivationEnabled()) {
    RecordUma(GetDisplayMediaTransientActivation::kMissingButFeatureDisabled);
    return true;
  }

  if (!settings->GetRequireTransientActivationForGetDisplayMedia()) {
    RecordUma(GetDisplayMediaTransientActivation::kMissingButPolicyOverrides);
    return true;
  }

  RecordUma(GetDisplayMediaTransientActivation::kMissing);
  return false;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
bool IsExtensionScreenSharingFunctionCall(const MediaStreamConstraints* options,
                                          ExceptionState& exception_state) {
  DCHECK(!exception_state.HadException());

  if (!options) {
    return false;
  }

  const V8UnionBooleanOrMediaTrackConstraints* const video = options->video();
  if (!video || video->GetContentType() !=
                    V8UnionBooleanOrMediaTrackConstraints::ContentType::
                        kMediaTrackConstraints) {
    return false;
  }

  const MediaTrackConstraints* const constraints =
      video->GetAsMediaTrackConstraints();
  if (!constraints || !constraints->hasMandatory()) {
    return false;
  }

  const HashMap<String, String> map =
      blink::Dictionary(constraints->mandatory())
          .GetOwnPropertiesAsStringHashMap(exception_state);

  return !exception_state.HadException() && map.Contains("chromeMediaSourceId");
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

MediaStreamConstraints* ToMediaStreamConstraints(
    const UserMediaStreamConstraints* source) {
  DCHECK(source);

  MediaStreamConstraints* const constraints = MediaStreamConstraints::Create();

  if (source->hasAudio()) {
    constraints->setAudio(source->audio());
  }

  if (source->hasVideo()) {
    constraints->setVideo(source->video());
  }

  return constraints;
}

MediaStreamConstraints* ToMediaStreamConstraints(
    const DisplayMediaStreamOptions* source) {
  MediaStreamConstraints* const constraints = MediaStreamConstraints::Create();
  if (source->hasAudio()) {
    constraints->setAudio(source->audio());
  }
  if (source->hasVideo()) {
    constraints->setVideo(source->video());
  }
  if (source->hasPreferCurrentTab()) {
    constraints->setPreferCurrentTab(source->preferCurrentTab());
  }
  if (source->hasController()) {
    constraints->setController(source->controller());
  }
  if (source->hasSelfBrowserSurface()) {
    constraints->setSelfBrowserSurface(source->selfBrowserSurface());
  }
  if (source->hasSystemAudio()) {
    constraints->setSystemAudio(source->systemAudio());
  }
  if (source->hasWindowAudio()) {
    constraints->setWindowAudio(source->windowAudio());
  }
  if (source->hasSurfaceSwitching()) {
    constraints->setSurfaceSwitching(source->surfaceSwitching());
  }
  if (source->hasMonitorTypeSurfaces()) {
    constraints->setMonitorTypeSurfaces(source->monitorTypeSurfaces());
  }
  return constraints;
}

bool EqualDeviceForDeviceChange(const WebMediaDeviceInfo& lhs,
                                const WebMediaDeviceInfo& rhs) {
  return lhs.device_id == rhs.device_id && lhs.label == rhs.label &&
         lhs.group_id == rhs.group_id && lhs.IsAvailable() == rhs.IsAvailable();
}

base::Token SubCaptureTargetIdToToken(const String& id) {
  if (id.empty()) {
    return base::Token();
  }

  const base::Uuid guid = base::Uuid::ParseLowercase(id.Ascii());
  DCHECK(guid.is_valid());

  const base::Token token = blink::GUIDToToken(guid);
  DCHECK(!token.is_zero());
  return token;
}

media::MediaPermission::Type ToMediaPermissionType(
    mojom::blink::MediaDeviceType media_device_type) {
  switch (media_device_type) {
    case mojom::blink::MediaDeviceType::kMediaAudioInput:
    case mojom::blink::MediaDeviceType::kMediaAudioOutput:
      return media::MediaPermission::Type::kAudioCapture;
    case mojom::blink::MediaDeviceType::kMediaVideoInput:
      return media::MediaPermission::Type::kVideoCapture;
    case mojom::blink::MediaDeviceType::kNumMediaDeviceTypes:
      NOTREACHED();
  }
}

void RecordUma(EnumerateDevicesGetUserMediaInteraction value) {
  base::UmaHistogramEnumeration(
      "Media.MediaDevices.EnumerateDevices.GetUserMediaInteraction", value);
}

void RecordUma(EnumerateDevicesFirstStateOnContextDestroyed value) {
  base::UmaHistogramEnumeration(
      "Media.MediaDevices.EnumerateDevices.FirstStateOnContextDestroyed",
      value);
}

}  // namespace

const char MediaDevices::kSupplementName[] = "MediaDevices";

MediaDevices* MediaDevices::mediaDevices(Navigator& navigator) {
  MediaDevices* supplement =
      Supplement<Navigator>::From<MediaDevices>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<MediaDevices>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

MediaDevices::MediaDevices(Navigator& navigator)
    : ActiveScriptWrappable<MediaDevices>({}),
      Supplement<Navigator>(navigator),
      ExecutionContextLifecycleObserver(navigator.DomWindow()),
      is_execution_context_active_(!!navigator.DomWindow()),
      dispatcher_host_(navigator.GetExecutionContext()),
      receiver_(this, navigator.DomWindow()) {}

MediaDevices::~MediaDevices() = default;

ScriptPromise<IDLSequence<MediaDeviceInfo>> MediaDevices::enumerateDevices(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateWebRTCMethodCount(RTCAPIName::kEnumerateDevices);
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current frame is detached.");
    return ScriptPromise<IDLSequence<MediaDeviceInfo>>();
  }

  auto tracer = std::make_unique<ScopedMediaStreamTracer>(
      "MediaDevices.EnumerateDevices");
  auto* result_tracker = MakeGarbageCollected<ScriptPromiseResolverWithTracker<
      EnumerateDevicesResult, IDLSequence<MediaDeviceInfo>>>(
      script_state, "Media.MediaDevices.EnumerateDevices", base::Seconds(4));
  const auto promise = result_tracker->Promise();

  enumerate_device_requests_.insert(result_tracker);

  LocalFrame* frame = LocalDOMWindow::From(script_state)->GetFrame();
  GetDispatcherHost(frame).EnumerateDevices(
      /*request_audio_input=*/true, /*request_video_input=*/true,
      /*request_audio_output=*/true,
      /*request_video_input_capabilities=*/true,
      /*request_audio_input_capabilities=*/
      base::FeatureList::IsEnabled(kEnumerateDevicesRequestAudioCapabilities),
      BindOnce(&MediaDevices::DevicesEnumerated, WrapPersistent(this),
               WrapPersistent(result_tracker), std::move(tracer)));
  return promise;
}

MediaTrackSupportedConstraints* MediaDevices::getSupportedConstraints() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return MediaTrackSupportedConstraints::Create();
}

ScriptPromise<MediaStream> MediaDevices::getUserMedia(
    ScriptState* script_state,
    const UserMediaStreamConstraints* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto tracer =
      std::make_unique<ScopedMediaStreamTracer>("MediaDevices.GetUserMedia");

  // This timeout of base::Seconds(8) is an initial value and based on the data
  // in Media.MediaDevices.GetUserMedia.Latency, it should be iterated upon.
  // Records the `Media.MediaDevices.GetUserMedia.Result2` histogram.
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolverWithTracker<UserMediaRequestResult, MediaStream>>(
      script_state, "Media.MediaDevices.GetUserMedia", base::Seconds(8));
  resolver->SetResultSuffix("Result2");
  const auto promise = resolver->Promise();

  DCHECK(options);  // Guaranteed by the default value in the IDL.
  DCHECK(!exception_state.HadException());

  MediaStreamConstraints* const constraints = ToMediaStreamConstraints(options);
  if (!constraints) {
    DCHECK(exception_state.HadException());
    resolver->RecordAndDetach(UserMediaRequestResult::kInvalidConstraints);
    return promise;
  }

  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  LocalFrame* local_frame = window ? window->GetFrame() : nullptr;
  if (local_frame && local_frame->IsAdScriptInStack()) {
    if (constraints->hasAudio()) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kAdScriptInStackOnMicrophoneRead);
    }
    if (constraints->hasVideo()) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kAdScriptInStackOnCameraRead);
    }
  }

  return SendUserMediaRequest(UserMediaRequestType::kUserMedia, resolver,
                              constraints, exception_state, std::move(tracer));
}

template <typename IDLResolvedType>
ScriptPromise<IDLResolvedType> MediaDevices::SendUserMediaRequest(
    UserMediaRequestType media_type,
    ScriptPromiseResolverWithTracker<UserMediaRequestResult, IDLResolvedType>*
        resolver,
    const MediaStreamConstraints* options,
    ExceptionState& exception_state,
    std::unique_ptr<ScopedMediaStreamTracer> tracer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!exception_state.HadException());

  auto promise = resolver->Promise();
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "No media device client available; "
        "is this a detached window?",
        UserMediaRequestResult::kContextDestroyed);
    return promise;
  }

  base::OnceCallback<void(const String&, CaptureController*)>
      on_success_follow_up;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (media_type == UserMediaRequestType::kDisplayMedia ||
      IsExtensionScreenSharingFunctionCall(options, exception_state)) {
    if (options->hasController()) {
      on_success_follow_up = BindOnce(
          &MediaDevices::EnqueueMicrotaskToCloseFocusWindowOfOpportunity,
          WrapWeakPersistent(this));
    } else {
      // TODO(crbug.com/1381949): Don't wait until the IPC round-trip and have
      // the browser process focus-switch upon starting the capture.
      on_success_follow_up =
          BindOnce(&MediaDevices::CloseFocusWindowOfOpportunity,
                   WrapWeakPersistent(this));
    }
  }

  if (exception_state.HadException()) {
    resolver->RecordAndDetach(UserMediaRequestResult::kInvalidConstraints);
    return promise;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  auto* callbacks =
      MakeGarbageCollected<PromiseResolverCallbacks<IDLResolvedType>>(
          media_type, resolver, std::move(on_success_follow_up),
          std::move(tracer));

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  UserMediaClient* user_media_client = UserMediaClient::From(window);

  UserMediaRequest* request =
      UserMediaRequest::Create(window, user_media_client, media_type, options,
                               callbacks, exception_state);
  if (!request) {
    DCHECK(exception_state.HadException());
    resolver->RecordAndDetach(UserMediaRequestResult::kInvalidConstraints);
    return promise;
  }

  String error_message;
  if (!request->IsSecureContextUse(error_message)) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kNotSupportedError, error_message,
        UserMediaRequestResult::kInsecureContext);
    return promise;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (media_type == UserMediaRequestType::kDisplayMedia) {
    window->ConsumeDisplayCaptureRequestToken();
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  request->Start();
  return promise;
}

ScriptPromise<IDLSequence<MediaStream>> MediaDevices::getAllScreensMedia(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto tracer = std::make_unique<ScopedMediaStreamTracer>(
      "MediaDevices.GetAllScreensMedia");

  // This timeout of base::Seconds(6) is an initial value and based on the data
  // in Media.MediaDevices.GetAllScreensMedia.Latency, it should be iterated
  // upon.
  // Records the `Media.MediaDevices.GetAllScreensMedia.Result2` histogram.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolverWithTracker<
      UserMediaRequestResult, IDLSequence<MediaStream>>>(
      script_state, "Media.MediaDevices.GetAllScreensMedia", base::Seconds(6));
  resolver->SetResultSuffix("Result2");
  auto promise = resolver->Promise();

  ExecutionContext* const context = GetExecutionContext();
  if (!context) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "No media device client available; is this a detached window?",
        UserMediaRequestResult::kContextDestroyed);
    return promise;
  }

  const bool capture_allowed_by_permissions_policy = context->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kAllScreensCapture,
      ReportOptions::kReportOnFailure);

  base::UmaHistogramEnumeration(
      "Media.Ui.GetAllScreensMedia.AllScreensCapturePolicyResult",
      capture_allowed_by_permissions_policy
          ? DisplayCapturePolicyResult::kAllowed
          : DisplayCapturePolicyResult::kDisallowed);

  if (context->IsIsolatedContext() && !capture_allowed_by_permissions_policy) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kNotAllowedError,
        "Access to the feature \"all-screenscapture\" is disallowed by "
        "permissions policy.",
        UserMediaRequestResult::kNotAllowedError);
    return promise;
  }

  CHECK(context->IsIsolatedContext());

  MediaStreamConstraints* constraints = MediaStreamConstraints::Create();
  constraints->setVideo(
      MakeGarbageCollected<V8UnionBooleanOrMediaTrackConstraints>(true));
  return SendUserMediaRequest(UserMediaRequestType::kAllScreensMedia, resolver,
                              constraints, exception_state, std::move(tracer));
}

ScriptPromise<MediaStream> MediaDevices::getDisplayMedia(
    ScriptState* script_state,
    const DisplayMediaStreamOptions* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LocalDOMWindow* const window = DomWindow();

  auto tracer =
      std::make_unique<ScopedMediaStreamTracer>("MediaDevices.GetDisplayMedia");

  // Using timeout of base::Seconds(30) based on the
  // Media.MediaDevices.GetDisplayMedia.Latency values.
  // Records the `Media.MediaDevices.GetDisplayMedia.Result2` histogram.
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolverWithTracker<UserMediaRequestResult, MediaStream>>(
      script_state, "Media.MediaDevices.GetDisplayMedia", base::Seconds(30),
      /*min_latency_bucket=*/base::Seconds(1),
      /*max_latency_bucket*/ base::Seconds(60), /*n_buckets=*/100);
  resolver->SetResultSuffix("Result2");
  resolver->SetLatencySuffix("Latency2");

  auto promise = resolver->Promise();

  if (!window) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "No local DOM window; is this a detached window?",
        UserMediaRequestResult::kContextDestroyed);
    return promise;
  }

  const bool capture_allowed_by_permissions_policy = window->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kDisplayCapture,
      ReportOptions::kReportOnFailure);

  base::UmaHistogramEnumeration(
      "Media.Ui.GetDisplayMedia.DisplayCapturePolicyResult",
      capture_allowed_by_permissions_policy
          ? DisplayCapturePolicyResult::kAllowed
          : DisplayCapturePolicyResult::kDisallowed);

  if (!capture_allowed_by_permissions_policy) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kNotAllowedError,
        "Access to the feature \"display-capture\" is disallowed by "
        "permissions policy.",
        UserMediaRequestResult::kNotAllowedError);
    return promise;
  }

  if (!TransientActivationRequirementSatisfied(window)) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "getDisplayMedia() requires transient activation (user gesture).",
        UserMediaRequestResult::kInvalidStateError);
    return promise;
  }

  if (CaptureController* const capture_controller =
          options->getControllerOr(nullptr)) {
    if (capture_controller->IsBound()) {
      resolver->RecordAndThrowDOMException(
          exception_state, DOMExceptionCode::kInvalidStateError,
          "A CaptureController object may only be used with a single "
          "getDisplayMedia() invocation.",
          UserMediaRequestResult::kInvalidStateError);
      return promise;
    }
    capture_controller->SetIsBound(true);
  }

  MediaStreamConstraints* const constraints = ToMediaStreamConstraints(options);
  if (!options->hasSelfBrowserSurface() &&
      (!options->hasPreferCurrentTab() || !options->preferCurrentTab())) {
    constraints->setSelfBrowserSurface(
        V8DisplayMediaIncludeOrExclude::Enum::kExclude);
  }

  if (options->hasPreferCurrentTab() && options->preferCurrentTab()) {
    UseCounter::Count(window,
                      WebFeature::kGetDisplayMediaWithPreferCurrentTabTrue);
  }

  return SendUserMediaRequest(UserMediaRequestType::kDisplayMedia, resolver,
                              constraints, exception_state, std::move(tracer));
}

ScriptPromise<MediaDeviceInfo> MediaDevices::selectAudioOutput(
    ScriptState* script_state,
    const AudioOutputOptions* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);

  if (!script_state->ContextIsValid() || !window || !window->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kAbortError,
        "No local DOM window; is this a detached window?");
    return EmptyPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolverWithTracker<
      AudioOutputSelectionResult, MediaDeviceInfo>>(
      script_state, "Media.MediaDevices.SelectAudioOutput", base::Seconds(8));
  if (!LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    resolver->Reject<DOMException>(
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "selectAudioOutput() requires transient "
            "activation (user gesture)."),
        AudioOutputSelectionResult::kNoUserActivation);
    return resolver->Promise();
  }

  GetDispatcherHost(window->GetFrame())
      .SelectAudioOutput(
          options->hasDeviceId() ? options->deviceId() : String(),
          BindOnce(&MediaDevices::OnSelectAudioOutputResult,
                   WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

void MediaDevices::OnSelectAudioOutputResult(
    ScriptPromiseResolverWithTracker<AudioOutputSelectionResult,
                                     MediaDeviceInfo>* resolver,
    mojom::blink::SelectAudioOutputResultPtr result) {
  if (result->status == mojom::blink::AudioOutputStatus::kSuccess) {
    MediaDeviceInfo* media_device_info = MakeGarbageCollected<MediaDeviceInfo>(
        String::FromUTF8(result->device_info.device_id),
        String::FromUTF8(result->device_info.label),
        String::FromUTF8(result->device_info.group_id),
        mojom::MediaDeviceType::kMediaAudioOutput);
    resolver->Resolve(media_device_info, AudioOutputSelectionResult::kSuccess);
    return;
  } else {
    String error_message;
    DOMExceptionCode exception_code = DOMExceptionCode::kUnknownError;
    AudioOutputSelectionResult result_enum =
        AudioOutputSelectionResult::kOtherError;

    switch (result->status) {
      case mojom::blink::AudioOutputStatus::kNoPermission:
        error_message = "Permission denied to select audio output.";
        exception_code = DOMExceptionCode::kNotAllowedError;
        result_enum = AudioOutputSelectionResult::kPermissionDenied;
        break;
      case mojom::blink::AudioOutputStatus::kNoDevices:
        error_message = "No audio output devices found.";
        exception_code = DOMExceptionCode::kNotFoundError;
        result_enum = AudioOutputSelectionResult::kNoDevices;
        break;
      case mojom::blink::AudioOutputStatus::kNotSupported:
        error_message = "Audio output is not supported.";
        exception_code = DOMExceptionCode::kInvalidStateError;
        result_enum = AudioOutputSelectionResult::kNotSupported;
        break;
      case mojom::blink::AudioOutputStatus::kNoUserActivation:
        error_message =
            "selectAudioOutput() requires transient activation (user gesture).";
        exception_code = DOMExceptionCode::kInvalidStateError;
        result_enum = AudioOutputSelectionResult::kNoUserActivation;
        break;
      default:
        error_message =
            "An unknown error occurred during audio output selection.";
        exception_code = DOMExceptionCode::kUnknownError;
        result_enum = AudioOutputSelectionResult::kOtherError;
    }

    resolver->Reject<DOMException>(
        MakeGarbageCollected<DOMException>(exception_code, error_message),
        result_enum);
  }
}

void MediaDevices::setCaptureHandleConfig(ScriptState* script_state,
                                          const CaptureHandleConfig* config,
                                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config->hasExposeOrigin());
  DCHECK(config->hasHandle());

  if (config->handle().length() > 1024) {
    exception_state.ThrowTypeError(
        "Handle length exceeds 1024 16-bit characters.");
    return;
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Current frame is detached.");
    return;
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window || !window->GetFrame()) {
    return;
  }

  if (!window->GetFrame()->IsOutermostMainFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Can only be called from the top-level document.");
    return;
  }

  auto config_ptr = mojom::blink::CaptureHandleConfig::New();
  config_ptr->expose_origin = config->exposeOrigin();
  config_ptr->capture_handle = config->handle();
  if (config->permittedOrigins().size() == 1 &&
      config->permittedOrigins()[0] == "*") {
    config_ptr->all_origins_permitted = true;
  } else {
    config_ptr->all_origins_permitted = false;
    config_ptr->permitted_origins.reserve(config->permittedOrigins().size());
    for (const auto& permitted_origin : config->permittedOrigins()) {
      if (permitted_origin == "*") {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          "Wildcard only valid in isolation.");
        return;
      }

      scoped_refptr<SecurityOrigin> origin =
          SecurityOrigin::CreateFromString(permitted_origin);
      if (!origin || origin->IsOpaque()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          "Invalid origin encountered.");
        return;
      }
      config_ptr->permitted_origins.emplace_back(std::move(origin));
    }
  }

  GetDispatcherHost(window->GetFrame())
      .SetCaptureHandleConfig(std::move(config_ptr));
}

ScriptPromise<IDLUndefined> MediaDevices::setPreferredSinkId(
    ScriptState* script_state,
    const String& sink_id,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateWebRTCMethodCount(RTCAPIName::kSetPreferredSinkId);

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Current frame is detached.");
    return ScriptPromise<IDLUndefined>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  LocalFrame* frame = LocalDOMWindow::From(script_state)->GetFrame();
  GetDispatcherHost(frame).SetPreferredSinkId(
      sink_id,
      BindOnce(&MediaDevices::SetPreferredSinkIdResultReceived,
               WrapWeakPersistent(this), sink_id, WrapPersistent(resolver)));

  return promise;
}

void MediaDevices::SetPreferredSinkIdResultReceived(
    const String& sink_id,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    media::mojom::blink::OutputDeviceStatus status) {
  auto* excecution_context = resolver->GetExecutionContext();
  if (!excecution_context || excecution_context->IsContextDestroyed()) {
    return;
  }

  switch (static_cast<media::OutputDeviceStatus>(status)) {
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK:
      resolver->Resolve();
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError,
          StrCat({"Device not found, ", sink_id, "."})));
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          StrCat({"Not authorized to access the device, ", sink_id, "."})));
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kTimeoutError,
          StrCat({"Timeout to access the device, ", sink_id, "."})));
      break;
    default:
      NOTREACHED();
  }
}

ScriptPromise<CropTarget> MediaDevices::ProduceCropTarget(
    ScriptState* script_state,
    Element* element,
    ExceptionState& exception_state) {
  if (!MayProduceSubCaptureTarget(script_state, element, exception_state,
                                  SubCaptureTarget::Type::kCropTarget)) {
    // Exception thrown by helper.
    return EmptyPromise();
  }

  if (const RegionCaptureCropId* id = element->GetRegionCaptureCropId()) {
    // A token was produced earlier and associated with the Element.
    const base::Token token = id->value();
    DCHECK(!token.is_zero());
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<CropTarget>>(
        script_state, exception_state.GetContext());
    const ScriptPromise<CropTarget> promise = resolver->Promise();
    const String token_str(blink::TokenToGUID(token).AsLowercaseString());
    resolver->Resolve(MakeGarbageCollected<CropTarget>(token_str));
    RecordUma(
        SubCaptureTarget::Type::kCropTarget,
        ProduceTargetFunctionResult::kDuplicateCallAfterPromiseResolution);
    return promise;
  }

  const auto it = crop_target_resolvers_.find(element);
  if (it != crop_target_resolvers_.end()) {
    // The Element does not yet have the SubCaptureTarget attached,
    // but the production of one has already been kicked off, and a response
    // will soon arrive from the browser process.
    // The Promise we return here will be resolved along with the original one.
    RecordUma(
        SubCaptureTarget::Type::kCropTarget,
        ProduceTargetFunctionResult::kDuplicateCallBeforePromiseResolution);
    return it->value->Promise();
  }

  // Mints a new ID on the browser process.
  // Resolves after it has been produced and is ready to be used.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<CropTarget>>(
      script_state, exception_state.GetContext());
  crop_target_resolvers_.insert(element, resolver);
  const ScriptPromise<CropTarget> promise = resolver->Promise();

  LocalDOMWindow* const window = To<LocalDOMWindow>(GetExecutionContext());
  CHECK(window);  // Guaranteed by MayProduceSubCaptureTarget() earlier.

  base::OnceCallback callback =
      BindOnce(&MediaDevices::ResolveCropTargetPromise, WrapPersistent(this),
               WrapPersistent(element));
  GetDispatcherHost(window->GetFrame())
      .ProduceSubCaptureTargetId(SubCaptureTarget::Type::kCropTarget,
                                 std::move(callback));
  RecordUma(SubCaptureTarget::Type::kCropTarget,
            ProduceTargetFunctionResult::kPromiseProduced);
  return promise;
}

ScriptPromise<RestrictionTarget> MediaDevices::ProduceRestrictionTarget(
    ScriptState* script_state,
    Element* element,
    ExceptionState& exception_state) {
  if (!MayProduceSubCaptureTarget(script_state, element, exception_state,
                                  SubCaptureTarget::Type::kRestrictionTarget)) {
    // Exception thrown by helper.
    return EmptyPromise();
  }

  if (const RestrictionTargetId* id = element->GetRestrictionTargetId()) {
    // A token was produced earlier and associated with the Element.
    const base::Token token = id->value();
    DCHECK(!token.is_zero());
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<RestrictionTarget>>(
            script_state, exception_state.GetContext());
    const ScriptPromise<RestrictionTarget> promise = resolver->Promise();
    const String token_str(blink::TokenToGUID(token).AsLowercaseString());
    resolver->Resolve(MakeGarbageCollected<RestrictionTarget>(token_str));
    RecordUma(
        SubCaptureTarget::Type::kRestrictionTarget,
        ProduceTargetFunctionResult::kDuplicateCallAfterPromiseResolution);
    return promise;
  }

  const auto it = restriction_target_resolvers_.find(element);
  if (it != restriction_target_resolvers_.end()) {
    // The Element does not yet have the SubCaptureTarget attached,
    // but the production of one has already been kicked off, and a response
    // will soon arrive from the browser process.
    // The Promise we return here will be resolved along with the original one.
    RecordUma(
        SubCaptureTarget::Type::kRestrictionTarget,
        ProduceTargetFunctionResult::kDuplicateCallBeforePromiseResolution);
    return it->value->Promise();
  }

  // Mints a new ID on the browser process.
  // Resolves after it has been produced and is ready to be used.
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<RestrictionTarget>>(
          script_state, exception_state.GetContext());
  restriction_target_resolvers_.insert(element, resolver);
  const ScriptPromise<RestrictionTarget> promise = resolver->Promise();

  LocalDOMWindow* const window = To<LocalDOMWindow>(GetExecutionContext());
  CHECK(window);  // Guaranteed by MayProduceSubCaptureTarget() earlier.

  base::OnceCallback callback =
      BindOnce(&MediaDevices::ResolveRestrictionTargetPromise,
               WrapPersistent(this), WrapPersistent(element));
  GetDispatcherHost(window->GetFrame())
      .ProduceSubCaptureTargetId(SubCaptureTarget::Type::kRestrictionTarget,
                                 std::move(callback));
  RecordUma(SubCaptureTarget::Type::kRestrictionTarget,
            ProduceTargetFunctionResult::kPromiseProduced);
  return promise;
}

const AtomicString& MediaDevices::InterfaceName() const {
  return event_target_names::kMediaDevices;
}

ExecutionContext* MediaDevices::GetExecutionContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void MediaDevices::RemoveAllEventListeners() {
  EventTarget::RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
  StopObserving();
}

void MediaDevices::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EventTarget::AddedEventListener(event_type, registered_listener);
  StartObserving();
}

void MediaDevices::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EventTarget::RemovedEventListener(event_type, registered_listener);
  if (!HasEventListeners()) {
    StopObserving();
  }
}

bool MediaDevices::HasPendingActivity() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return receiver_.is_bound();
}

void MediaDevices::ContextDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_execution_context_active_) {
    return;
  }

  is_execution_context_active_ = false;
  enumerate_device_requests_.clear();
  StopObserving();

  switch (first_ed_state_) {
    case FirstEnumerateDevicesState::kNoEnumeration:
      break;
    case FirstEnumerateDevicesState::kFailed:
      RecordUma(EnumerateDevicesFirstStateOnContextDestroyed::kFailed);
      break;
    case FirstEnumerateDevicesState::kSuccessful:
      switch (first_gum_state_) {
        case FirstGetUserMediaState::kNoGetUserMedia:
          RecordUma(EnumerateDevicesFirstStateOnContextDestroyed::
                        kSuccessfulNeverGetUserMedia);
          break;
        case FirstGetUserMediaState::kBeforeEnumerateDevices:
          RecordUma(EnumerateDevicesFirstStateOnContextDestroyed::
                        kSuccessfulAfterGetUserMedia);
          break;
        case FirstGetUserMediaState::kAfterEnumerateDevices:
          RecordUma(EnumerateDevicesFirstStateOnContextDestroyed::
                        kSuccessfulFollowedByGetUserMedia);
          break;
      }
      break;
  }
}

void MediaDevices::OnDevicesChanged(
    mojom::blink::MediaDeviceType type,
    const Vector<WebMediaDeviceInfo>& device_infos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetExecutionContext());
  CHECK(is_execution_context_active_);
  if (std::ranges::equal(current_device_infos_[static_cast<wtf_size_t>(type)],
                         device_infos, EqualDeviceForDeviceChange)) {
    return;
  }

  current_device_infos_[static_cast<wtf_size_t>(type)] = device_infos;
  if (DomWindow()
          ->GetFrame()
          ->GetSettings()
          ->GetIgnorePermissionForDeviceChangedEvent()) {
    MaybeFireDeviceChangeEvent(/*has_permission=*/true);
    return;
  }

  if (media::MediaPermission* media_permission =
          blink::Platform::Current()->GetWebRTCMediaPermission(
              WebLocalFrame::FromFrameToken(
                  DomWindow()->GetLocalFrameToken()))) {
    media_permission->HasPermission(
        ToMediaPermissionType(type),
        BindOnce(&MediaDevices::MaybeFireDeviceChangeEvent,
                 WrapWeakPersistent(this)));
  }
}

void MediaDevices::MaybeFireDeviceChangeEvent(bool has_permission) {
  if (has_permission ||
      !base::FeatureList::IsEnabled(kDeviceChangeRequiresPermission)) {
    ScheduleDispatchEvent(Event::Create(event_type_names::kDevicechange));
  }
}

void MediaDevices::ScheduleDispatchEvent(Event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_execution_context_active_) {
    return;
  }

  scheduled_events_.push_back(event);
  if (dispatch_scheduled_events_task_handle_.IsActive()) {
    return;
  }

  auto* context = GetExecutionContext();
  DCHECK(context);
  dispatch_scheduled_events_task_handle_ = PostCancellableTask(
      *context->GetTaskRunner(TaskType::kMediaElementEvent), FROM_HERE,
      BindOnce(&MediaDevices::DispatchScheduledEvents, WrapPersistent(this)));
}

void MediaDevices::DispatchScheduledEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_execution_context_active_) {
    return;
  }
  HeapVector<Member<Event>> events;
  events.swap(scheduled_events_);

  for (const auto& event : events) {
    DispatchEvent(*event);
  }
}

void MediaDevices::StartObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (receiver_.is_bound() || !is_execution_context_active_ ||
      starting_observation_) {
    return;
  }

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window) {
    return;
  }

  starting_observation_ = true;
  GetDispatcherHost(window->GetFrame())
      .EnumerateDevices(/*request_audio_input=*/true,
                        /*request_video_input=*/true,
                        /*request_audio_output=*/true,
                        /*request_video_input_capabilities=*/false,
                        /*request_audio_input_capabilities=*/false,
                        BindOnce(&MediaDevices::FinalizeStartObserving,
                                 WrapPersistent(this)));
}

void MediaDevices::FinalizeStartObserving(
    const Vector<Vector<WebMediaDeviceInfo>>& enumeration,
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities,
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  starting_observation_ = false;
  if (receiver_.is_bound() || !is_execution_context_active_) {
    return;
  }

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window) {
    return;
  }

  current_device_infos_ = enumeration;

  GetDispatcherHost(window->GetFrame())
      .AddMediaDevicesListener(true /* audio input */, true /* video input */,
                               true /* audio output */,
                               receiver_.BindNewPipeAndPassRemote(
                                   GetExecutionContext()->GetTaskRunner(
                                       TaskType::kMediaElementEvent)));
}

void MediaDevices::StopObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!receiver_.is_bound()) {
    return;
  }
  receiver_.reset();
}

void MediaDevices::DevicesEnumerated(
    ScriptPromiseResolverWithTracker<EnumerateDevicesResult,
                                     IDLSequence<MediaDeviceInfo>>*
        result_tracker,
    std::unique_ptr<ScopedMediaStreamTracer> tracer,
    const Vector<Vector<WebMediaDeviceInfo>>& enumeration,
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities,
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enumerate_device_requests_.Contains(result_tracker)) {
    return;
  }

  enumerate_device_requests_.erase(result_tracker);

  ScriptState* script_state = result_tracker->GetScriptState();
  if (!script_state || !ExecutionContext::From(script_state) ||
      ExecutionContext::From(script_state)->IsContextDestroyed()) {
    return;
  }

  DCHECK_EQ(static_cast<wtf_size_t>(
                mojom::blink::MediaDeviceType::kNumMediaDeviceTypes),
            enumeration.size());

  if (!video_input_capabilities.empty()) {
    DCHECK_EQ(enumeration[static_cast<wtf_size_t>(
                              mojom::blink::MediaDeviceType::kMediaVideoInput)]
                  .size(),
              video_input_capabilities.size());
  }
  if (!audio_input_capabilities.empty()) {
    DCHECK_EQ(enumeration[static_cast<wtf_size_t>(
                              mojom::blink::MediaDeviceType::kMediaAudioInput)]
                  .size(),
              audio_input_capabilities.size());
  }

  MediaDeviceInfoVector media_devices;
  bool result_contains_nonempty_input_device_ids = false;
  for (wtf_size_t i = 0;
       i < static_cast<wtf_size_t>(
               mojom::blink::MediaDeviceType::kNumMediaDeviceTypes);
       ++i) {
    for (wtf_size_t j = 0; j < enumeration[i].size(); ++j) {
      mojom::blink::MediaDeviceType device_type =
          static_cast<mojom::blink::MediaDeviceType>(i);
      WebMediaDeviceInfo device_info = enumeration[i][j];
      String device_label = String::FromUTF8(device_info.label);
      if (device_type == mojom::blink::MediaDeviceType::kMediaAudioInput ||
          device_type == mojom::blink::MediaDeviceType::kMediaVideoInput) {
        if (!device_info.device_id.empty()) {
          result_contains_nonempty_input_device_ids = true;
        }
        InputDeviceInfo* input_device_info =
            MakeGarbageCollected<InputDeviceInfo>(
                String::FromUTF8(device_info.device_id), device_label,
                String::FromUTF8(device_info.group_id), device_type);
        if (device_type == mojom::blink::MediaDeviceType::kMediaVideoInput &&
            !video_input_capabilities.empty()) {
          input_device_info->SetVideoInputCapabilities(
              std::move(video_input_capabilities[j]));
        }
        if (device_type == mojom::blink::MediaDeviceType::kMediaAudioInput &&
            !audio_input_capabilities.empty()) {
          input_device_info->SetAudioInputCapabilities(
              std::move(audio_input_capabilities[j]));
        }
        media_devices.push_back(input_device_info);
      } else {
        media_devices.push_back(MakeGarbageCollected<MediaDeviceInfo>(
            String::FromUTF8(device_info.device_id), device_label,
            String::FromUTF8(device_info.group_id), device_type));
      }
    }
  }

  ReportCompletedEnumerateDevices(result_contains_nonempty_input_device_ids);
  result_tracker->Resolve(media_devices);
  tracer->End();
}

void MediaDevices::OnDispatcherHostConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (ScriptPromiseResolverWithTracker<EnumerateDevicesResult,
                                        IDLSequence<MediaDeviceInfo>>*
           result_tracker : enumerate_device_requests_) {
    result_tracker->Reject<DOMException>(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                           "enumerateDevices() failed."),
        EnumerateDevicesResult::kErrorMediaDevicesDispatcherHostDisconnected);
  }
  enumerate_device_requests_.clear();
  dispatcher_host_.reset();
}

mojom::blink::MediaDevicesDispatcherHost& MediaDevices::GetDispatcherHost(
    LocalFrame* frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* const execution_context = GetExecutionContext();
  DCHECK(execution_context);

  if (!dispatcher_host_.is_bound()) {
    // Note: kInternalMediaRealTime is a better candidate for this job,
    // but kMediaElementEvent is used for consistency.
    frame->GetBrowserInterfaceBroker().GetInterface(
        dispatcher_host_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMediaElementEvent)));
    dispatcher_host_.set_disconnect_handler(
        BindOnce(&MediaDevices::OnDispatcherHostConnectionError,
                 WrapWeakPersistent(this)));
  }

  DCHECK(dispatcher_host_.get());
  return *dispatcher_host_.get();
}

void MediaDevices::SetDispatcherHostForTesting(
    mojo::PendingRemote<mojom::blink::MediaDevicesDispatcherHost>
        dispatcher_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* const execution_context = GetExecutionContext();
  DCHECK(execution_context);

  dispatcher_host_.Bind(
      std::move(dispatcher_host),
      execution_context->GetTaskRunner(TaskType::kMediaElementEvent));
  dispatcher_host_.set_disconnect_handler(
      BindOnce(&MediaDevices::OnDispatcherHostConnectionError,
               WrapWeakPersistent(this)));
}

void MediaDevices::Trace(Visitor* visitor) const {
  visitor->Trace(dispatcher_host_);
  visitor->Trace(receiver_);
  visitor->Trace(scheduled_events_);
  visitor->Trace(enumerate_device_requests_);

  visitor->Trace(crop_target_resolvers_);
  visitor->Trace(restriction_target_resolvers_);

  Supplement<Navigator>::Trace(visitor);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void MediaDevices::EnqueueMicrotaskToCloseFocusWindowOfOpportunity(
    const String& id,
    CaptureController* capture_controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* const context = GetExecutionContext();
  if (!context) {
    return;
  }

  context->GetAgent()->event_loop()->EnqueueMicrotask(BindOnce(
      &MediaDevices::CloseFocusWindowOfOpportunity, WrapWeakPersistent(this),
      id, WrapWeakPersistent(capture_controller)));
}

void MediaDevices::CloseFocusWindowOfOpportunity(
    const String& id,
    CaptureController* capture_controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* const context = GetExecutionContext();
  if (!context) {
    return;  // Note: We're still back by the browser-side timer.
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(context);
  if (!window) {
    return;
  }

  if (capture_controller) {
    capture_controller->FinalizeFocusDecision();
  }

  GetDispatcherHost(window->GetFrame()).CloseFocusWindowOfOpportunity(id);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

void MediaDevices::ResolveRestrictionTargetPromise(Element* element,
                                                   const String& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(element);  // Persistent.

  const auto it = restriction_target_resolvers_.find(element);
  CHECK_NE(it, restriction_target_resolvers_.end());
  ScriptPromiseResolver<RestrictionTarget>* const resolver = it->value;
  restriction_target_resolvers_.erase(it);

  const base::Token token = SubCaptureTargetIdToToken(id);
  if (token.is_zero()) {
    resolver->Reject();
    RecordUma(SubCaptureTarget::Type::kRestrictionTarget,
              ProduceTargetPromiseResult::kPromiseRejected);
    return;
  }

  element->SetRestrictionTargetId(std::make_unique<RestrictionTargetId>(token));
  resolver->Resolve(MakeGarbageCollected<RestrictionTarget>(id));
  RecordUma(SubCaptureTarget::Type::kRestrictionTarget,
            ProduceTargetPromiseResult::kPromiseResolved);
}

// Checks whether the production of a SubCaptureTarget of the given type is
// allowed. Throw an appropriate exception if not.
bool MediaDevices::MayProduceSubCaptureTarget(ScriptState* script_state,
                                              Element* element,
                                              ExceptionState& exception_state,
                                              SubCaptureTarget::Type type) {
  CHECK(type == SubCaptureTarget::Type::kCropTarget ||
        type == SubCaptureTarget::Type::kRestrictionTarget);

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current frame is detached.");
    RecordUma(type, ProduceTargetFunctionResult::kInvalidContext);
    return false;
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window) {
    RecordUma(type, ProduceTargetFunctionResult::kGenericError);
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Missing execution context.");
    return false;
  }

  if (!element) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid element.");
    return false;
  }

  if (GetExecutionContext() != element->GetExecutionContext()) {
    RecordUma(type, ProduceTargetFunctionResult::
                        kElementAndMediaDevicesNotInSameExecutionContext);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The Element and the MediaDevices object must be same-window.");
    return false;
  }

  return true;
}

void MediaDevices::ResolveCropTargetPromise(Element* element,
                                            const String& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(element);  // Persistent.

  const auto it = crop_target_resolvers_.find(element);
  CHECK_NE(it, crop_target_resolvers_.end());
  ScriptPromiseResolver<CropTarget>* const resolver = it->value;
  crop_target_resolvers_.erase(it);

  const base::Token token = SubCaptureTargetIdToToken(id);
  if (token.is_zero()) {
    resolver->Reject();
    RecordUma(SubCaptureTarget::Type::kCropTarget,
              ProduceTargetPromiseResult::kPromiseRejected);
    return;
  }

  element->SetRegionCaptureCropId(std::make_unique<RegionCaptureCropId>(token));
  resolver->Resolve(MakeGarbageCollected<CropTarget>(id));
  RecordUma(SubCaptureTarget::Type::kCropTarget,
            ProduceTargetPromiseResult::kPromiseResolved);
}

void MediaDevices::ReportSuccessfulGetUserMedia() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (first_gum_state_ != FirstGetUserMediaState::kNoGetUserMedia) {
    return;
  }

  switch (first_ed_state_) {
    case FirstEnumerateDevicesState::kNoEnumeration:
      first_gum_state_ = FirstGetUserMediaState::kBeforeEnumerateDevices;
      RecordUma(EnumerateDevicesGetUserMediaInteraction::kGetUserMediaFirst);
      break;
    case FirstEnumerateDevicesState::kFailed:
      first_gum_state_ = FirstGetUserMediaState::kAfterEnumerateDevices;
      RecordUma(EnumerateDevicesGetUserMediaInteraction::
                    kFailedEnumerateDevicesThenGetUserMedia);
      break;
    case FirstEnumerateDevicesState::kSuccessful:
      first_gum_state_ = FirstGetUserMediaState::kAfterEnumerateDevices;
      RecordUma(EnumerateDevicesGetUserMediaInteraction::
                    kSuccessfulEnumerateDevicesThenGetUserMedia);
      break;
  }
}

void MediaDevices::ReportCompletedEnumerateDevices(bool is_successful) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (first_ed_state_ != FirstEnumerateDevicesState::kNoEnumeration) {
    return;
  }

  if (is_successful) {
    first_ed_state_ = FirstEnumerateDevicesState::kSuccessful;
    switch (first_gum_state_) {
      case FirstGetUserMediaState::kNoGetUserMedia:
        RecordUma(EnumerateDevicesGetUserMediaInteraction::
                      kSuccessfulEnumerateDevicesFirst);
        break;
      case FirstGetUserMediaState::kBeforeEnumerateDevices:
        RecordUma(EnumerateDevicesGetUserMediaInteraction::
                      kGetUserMediaThenSuccessfulEnumerateDevices);
        break;
      case FirstGetUserMediaState::kAfterEnumerateDevices:
        NOTREACHED();
    }
  } else {
    first_ed_state_ = FirstEnumerateDevicesState::kFailed;
    switch (first_gum_state_) {
      case FirstGetUserMediaState::kNoGetUserMedia:
        RecordUma(EnumerateDevicesGetUserMediaInteraction::
                      kFailedEnumerateDevicesFirst);
        break;
      case FirstGetUserMediaState::kBeforeEnumerateDevices:
        RecordUma(EnumerateDevicesGetUserMediaInteraction::
                      kGetUserMediaThenFailedEnumerateDevices);
        break;
      case FirstGetUserMediaState::kAfterEnumerateDevices:
        NOTREACHED();
    }
  }
}

}  // namespace blink
