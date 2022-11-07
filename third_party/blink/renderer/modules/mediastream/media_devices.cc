// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_devices.h"

#include <utility>

#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_display_media_stream_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_supported_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_domexception_overconstrainederror.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_user_media_stream_constraints.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/identifiability_metrics.h"
#include "third_party/blink/renderer/modules/mediastream/input_device_info.h"
#include "third_party/blink/renderer/modules/mediastream/media_error_state.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/navigator_media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"display-capture\" is disallowed by permission "
    "policy.";

class PromiseResolverCallbacks final : public UserMediaRequest::Callbacks {
 public:
  PromiseResolverCallbacks(
      UserMediaRequestType media_type,
      ScriptPromiseResolver* resolver,
      base::OnceCallback<void(const String&, MediaStreamTrack*)>
          on_success_follow_up)
      : media_type_(media_type),
        resolver_(resolver),
        on_success_follow_up_(std::move(on_success_follow_up)) {}
  ~PromiseResolverCallbacks() override = default;

  void OnSuccess(const MediaStreamVector& streams) override {
    if (media_type_ == UserMediaRequestType::kDisplayMediaSet) {
      OnSuccessGetDisplayMediaSet(streams);
      return;
    }

    DCHECK_EQ(streams.size(), 1u);
    MediaStream* stream = streams[0];

    MediaStreamTrack* video_track = nullptr;

    if (on_success_follow_up_) {
      // Only getDisplayMedia() calls set |on_success_follow_up_|.
      // Successful invocations of getDisplayMedia() always have exactly
      // one video track.
      DCHECK_EQ(UserMediaRequestType::kDisplayMedia, media_type_);
      MediaStreamTrackVector video_tracks = stream->getVideoTracks();
      DCHECK_EQ(video_tracks.size(), 1u);
      video_track = video_tracks[0];
    }

    // Resolve Promise<MediaStream> on a microtask.
    resolver_->Resolve(stream);

    // Enqueue the follow-up microtask, if any is intended.
    if (on_success_follow_up_ && video_track) {
      std::move(on_success_follow_up_).Run(stream->id(), video_track);
    }
  }

  void OnError(ScriptWrappable* callback_this_value,
               const V8MediaStreamError* error) override {
    resolver_->Reject(error);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    UserMediaRequest::Callbacks::Trace(visitor);
  }

 private:
  void OnSuccessGetDisplayMediaSet(const MediaStreamVector& streams) {
    DCHECK(!streams.empty());
    DCHECK_EQ(UserMediaRequestType::kDisplayMediaSet, media_type_);
    resolver_->Resolve(streams);
  }

  const UserMediaRequestType media_type_;

  Member<ScriptPromiseResolver> resolver_;
  base::OnceCallback<void(const String&, MediaStreamTrack*)>
      on_success_follow_up_;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DisplayCapturePolicyResult {
  kDisallowed = 0,
  kAllowed = 1,
  kMaxValue = kAllowed
};

#if !BUILDFLAG(IS_ANDROID)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProduceCropTargetFunctionResult {
  kPromiseProduced = 0,
  kGenericError = 1,
  kInvalidContext = 2,
  kDuplicateCallBeforePromiseResolution = 3,
  kDuplicateCallAfterPromiseResolution = 4,
  kElementAndMediaDevicesNotInSameExecutionContext = 5,
  kMaxValue = kElementAndMediaDevicesNotInSameExecutionContext
};

void RecordUma(ProduceCropTargetFunctionResult result) {
  base::UmaHistogramEnumeration(
      "Media.RegionCapture.ProduceCropTarget.Function.Result", result);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProduceCropTargetPromiseResult {
  kPromiseResolved = 0,
  kPromiseRejected = 1,
  kMaxValue = kPromiseRejected
};

void RecordUma(ProduceCropTargetPromiseResult result) {
  base::UmaHistogramEnumeration(
      "Media.RegionCapture.ProduceCropTarget.Promise.Result", result);
}
#endif

void RecordEnumerateDevicesLatency(base::TimeTicks start_time) {
  const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes("WebRTC.EnumerateDevices.Latency", elapsed);
}

MediaStreamConstraints* ToMediaStreamConstraints(
    const UserMediaStreamConstraints* source) {
  MediaStreamConstraints* options = MediaStreamConstraints::Create();
  if (source->hasAudio())
    options->setAudio(source->audio());
  if (source->hasVideo())
    options->setVideo(source->video());
  return options;
}

MediaStreamConstraints* ToMediaStreamConstraints(
    const DisplayMediaStreamOptions* source) {
  MediaStreamConstraints* options = MediaStreamConstraints::Create();
  if (source->hasAudio())
    options->setAudio(source->audio());
  if (source->hasVideo())
    options->setVideo(source->video());
  if (source->hasPreferCurrentTab())
    options->setPreferCurrentTab(source->preferCurrentTab());
  if (source->hasAutoSelectAllScreens())
    options->setAutoSelectAllScreens(source->autoSelectAllScreens());
  if (source->hasSystemAudio())
    options->setSystemAudio(source->systemAudio());
  if (source->hasSelfBrowserSurface())
    options->setSelfBrowserSurface(source->selfBrowserSurface());
  if (source->hasSurfaceSwitching())
    options->setSurfaceSwitching(source->surfaceSwitching());
  return options;
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
    : Supplement<Navigator>(navigator),
      ExecutionContextLifecycleObserver(navigator.DomWindow()),
      stopped_(false),
      dispatcher_host_(navigator.GetExecutionContext()),
      receiver_(this, navigator.DomWindow()) {}

MediaDevices::~MediaDevices() = default;

ScriptPromise MediaDevices::enumerateDevices(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  UpdateWebRTCMethodCount(RTCAPIName::kEnumerateDevices);
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current frame is detached.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  enumerate_device_requests_.Set(
      resolver, RequestMetadata{.start_time = base::TimeTicks::Now()});

  LocalFrame* frame = LocalDOMWindow::From(script_state)->GetFrame();
  GetDispatcherHost(frame).EnumerateDevices(
      true /* audio input */, true /* video input */, true /* audio output */,
      true /* request_video_input_capabilities */,
      true /* request_audio_input_capabilities */,
      WTF::BindOnce(&MediaDevices::DevicesEnumerated, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return promise;
}

MediaTrackSupportedConstraints* MediaDevices::getSupportedConstraints() const {
  return MediaTrackSupportedConstraints::Create();
}

ScriptPromise MediaDevices::getUserMedia(
    ScriptState* script_state,
    const UserMediaStreamConstraints* options,
    ExceptionState& exception_state) {
  return SendUserMediaRequest(script_state, UserMediaRequestType::kUserMedia,
                              ToMediaStreamConstraints(options),
                              exception_state);
}

ScriptPromise MediaDevices::SendUserMediaRequest(
    ScriptState* script_state,
    UserMediaRequestType media_type,
    const MediaStreamConstraints* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "No media device client available; "
                                      "is this a detached window?");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  base::OnceCallback<void(const String&, MediaStreamTrack*)>
      on_success_follow_up;
#if !BUILDFLAG(IS_ANDROID)
  if (media_type == UserMediaRequestType::kDisplayMedia) {
    on_success_follow_up = WTF::BindOnce(
        &MediaDevices::EnqueueMicrotaskToCloseFocusWindowOfOpportunity,
        WrapWeakPersistent(this));
  }
#endif

  auto* callbacks = MakeGarbageCollected<PromiseResolverCallbacks>(
      media_type, resolver, std::move(on_success_follow_up));

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  UserMediaClient* user_media_client = UserMediaClient::From(window);
  constexpr IdentifiableSurface::Type surface_type =
      IdentifiableSurface::Type::kMediaDevices_GetUserMedia;
  IdentifiableSurface surface;
  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(surface_type)) {
    surface = IdentifiableSurface::FromTypeAndToken(
        surface_type, TokenFromConstraints(options));
  }
  MediaErrorState error_state;
  UserMediaRequest* request =
      UserMediaRequest::Create(window, user_media_client, media_type, options,
                               callbacks, error_state, surface);
  if (!request) {
    DCHECK(error_state.HadException());
    if (error_state.CanGenerateException()) {
      error_state.RaiseException(exception_state);
      return ScriptPromise();
    }
    ScriptPromise rejected_promise = resolver->Promise();
    RecordIdentifiabilityMetric(
        surface, GetExecutionContext(),
        IdentifiabilityBenignStringToken(error_state.GetErrorMessage()));
    resolver->Reject(error_state.CreateError());
    return rejected_promise;
  }

  String error_message;
  if (!request->IsSecureContextUse(error_message)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      error_message);
    return ScriptPromise();
  }
  auto promise = resolver->Promise();
  request->Start();
  return promise;
}

ScriptPromise MediaDevices::getDisplayMediaSet(
    ScriptState* script_state,
    const DisplayMediaStreamOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* const context = GetExecutionContext();
  if (!context) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "No media device client available; is this a detached window?");
    return ScriptPromise();
  }

  return SendUserMediaRequest(
      script_state, UserMediaRequestType::kDisplayMediaSet,
      ToMediaStreamConstraints(options), exception_state);
}

ScriptPromise MediaDevices::getDisplayMedia(
    ScriptState* script_state,
    const DisplayMediaStreamOptions* options,
    ExceptionState& exception_state) {
  LocalDOMWindow* const window = DomWindow();
  if (!window) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "No local DOM window; is this a detached window?");
    return ScriptPromise();
  }

  // Measure calls without transient activation as required by spec in
  // https://github.com/w3c/mediacapture-screen-share/pull/106
  if (!LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    UseCounter::Count(window,
                      WebFeature::kGetDisplayMediaWithoutUserActivation);
  }

  const bool capture_allowed_by_permissions_policy = window->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kDisplayCapture,
      ReportOptions::kReportOnFailure);

  base::UmaHistogramEnumeration(
      "Media.Ui.GetDisplayMedia.DisplayCapturePolicyResult",
      capture_allowed_by_permissions_policy
          ? DisplayCapturePolicyResult::kAllowed
          : DisplayCapturePolicyResult::kDisallowed);

  if (!capture_allowed_by_permissions_policy) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      kFeaturePolicyBlocked);
    return ScriptPromise();
  }

  if (options->hasAutoSelectAllScreens() && options->autoSelectAllScreens()) {
    exception_state.ThrowTypeError(
        "The autoSelectAllScreens property is not allowed for usage with "
        "getDisplayMedia.");
    return ScriptPromise();
  }

  MediaStreamConstraints* const constraints = ToMediaStreamConstraints(options);
  if (base::FeatureList::IsEnabled(
          blink::features::kNewGetDisplayMediaPickerOrder) &&
      !options->hasSelfBrowserSurface() &&
      (!options->hasPreferCurrentTab() || !options->preferCurrentTab())) {
    constraints->setSelfBrowserSurface("exclude");
  }

  return SendUserMediaRequest(script_state, UserMediaRequestType::kDisplayMedia,
                              constraints, exception_state);
}

void MediaDevices::setCaptureHandleConfig(ScriptState* script_state,
                                          const CaptureHandleConfig* config,
                                          ExceptionState& exception_state) {
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

ScriptPromise MediaDevices::ProduceCropTarget(ScriptState* script_state,
                                              Element* element,
                                              ExceptionState& exception_state) {
  DCHECK(IsMainThread());

#if BUILDFLAG(IS_ANDROID)
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Unsupported.");
  return ScriptPromise();
#else
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current frame is detached.");
    RecordUma(ProduceCropTargetFunctionResult::kInvalidContext);
    return ScriptPromise();
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window) {
    RecordUma(ProduceCropTargetFunctionResult::kGenericError);
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Missing execution context.");
    return ScriptPromise();
  }

  if (!element || !element->IsSupportedByRegionCapture()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Support for this subtype is not yet implemented.");
    return ScriptPromise();
  }

  if (GetExecutionContext() != element->GetExecutionContext()) {
    RecordUma(ProduceCropTargetFunctionResult::
                  kElementAndMediaDevicesNotInSameExecutionContext);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The Element and the MediaDevices object must be same-window.");
    return ScriptPromise();
  }

  const RegionCaptureCropId* const old_crop_id =
      element->GetRegionCaptureCropId();
  if (old_crop_id) {
    // The Element has a crop-ID which was previously produced.
    DCHECK(!old_crop_id->value().is_zero());
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    const ScriptPromise promise = resolver->Promise();
    resolver->Resolve(MakeGarbageCollected<CropTarget>(WTF::String(
        blink::TokenToGUID(old_crop_id->value()).AsLowercaseString())));
    RecordUma(
        ProduceCropTargetFunctionResult::kDuplicateCallAfterPromiseResolution);
    return promise;
  }

  const auto it = crop_id_resolvers_.find(element);
  if (it != crop_id_resolvers_.end()) {
    // The Element does not yet have a crop-ID, but the production of one
    // has already been kicked off, and a response will soon arrive from
    // the browser process. The Promise we return here will be resolved along
    // with the original one.
    RecordUma(
        ProduceCropTargetFunctionResult::kDuplicateCallBeforePromiseResolution);
    return it->value->Promise();
  }

  // Mints a new crop-ID on the browser process. Resolve when it's produced
  // and ready to be used.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  crop_id_resolvers_.insert(element, resolver);
  const ScriptPromise promise = resolver->Promise();
  GetDispatcherHost(window->GetFrame())
      .ProduceCropId(WTF::BindOnce(&MediaDevices::ResolveProduceCropIdPromise,
                                   WrapPersistent(this),
                                   WrapPersistent(element)));
  RecordUma(ProduceCropTargetFunctionResult::kPromiseProduced);
  return promise;
#endif
}

const AtomicString& MediaDevices::InterfaceName() const {
  return event_target_names::kMediaDevices;
}

ExecutionContext* MediaDevices::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void MediaDevices::RemoveAllEventListeners() {
  EventTargetWithInlineData::RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
  StopObserving();
}

void MediaDevices::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  StartObserving();
}

void MediaDevices::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::RemovedEventListener(event_type,
                                                  registered_listener);
  if (!HasEventListeners())
    StopObserving();
}

bool MediaDevices::HasPendingActivity() const {
  DCHECK(stopped_ || receiver_.is_bound() == HasEventListeners());
  return receiver_.is_bound();
}

void MediaDevices::ContextDestroyed() {
  if (stopped_)
    return;

  stopped_ = true;
  enumerate_device_requests_.clear();
}

void MediaDevices::OnDevicesChanged(
    mojom::blink::MediaDeviceType type,
    const Vector<WebMediaDeviceInfo>& device_infos) {
  DCHECK(GetExecutionContext());

  if (RuntimeEnabledFeatures::OnDeviceChangeEnabled())
    ScheduleDispatchEvent(Event::Create(event_type_names::kDevicechange));

  if (device_change_test_callback_)
    std::move(device_change_test_callback_).Run();
}

void MediaDevices::ScheduleDispatchEvent(Event* event) {
  scheduled_events_.push_back(event);
  if (dispatch_scheduled_events_task_handle_.IsActive())
    return;

  auto* context = GetExecutionContext();
  DCHECK(context);
  dispatch_scheduled_events_task_handle_ = PostCancellableTask(
      *context->GetTaskRunner(TaskType::kMediaElementEvent), FROM_HERE,
      WTF::BindOnce(&MediaDevices::DispatchScheduledEvents,
                    WrapPersistent(this)));
}

void MediaDevices::DispatchScheduledEvents() {
  if (stopped_)
    return;
  HeapVector<Member<Event>> events;
  events.swap(scheduled_events_);

  for (const auto& event : events)
    DispatchEvent(*event);
}

void MediaDevices::StartObserving() {
  if (receiver_.is_bound() || stopped_)
    return;

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window)
    return;

  GetDispatcherHost(window->GetFrame())
      .AddMediaDevicesListener(true /* audio input */, true /* video input */,
                               true /* audio output */,
                               receiver_.BindNewPipeAndPassRemote(
                                   GetExecutionContext()->GetTaskRunner(
                                       TaskType::kMediaElementEvent)));
}

void MediaDevices::StopObserving() {
  if (!receiver_.is_bound())
    return;
  receiver_.reset();
}

namespace {

void RecordEnumeratedDevices(ScriptPromiseResolver* resolver,
                             const MediaDeviceInfoVector& media_devices) {
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleWebFeature(
          WebFeature::kIdentifiabilityMediaDevicesEnumerateDevices)) {
    return;
  }
  Document* document = LocalDOMWindow::From(resolver->GetScriptState())
                           ->GetFrame()
                           ->GetDocument();
  IdentifiableTokenBuilder builder;
  for (const auto& device_info : media_devices) {
    // Ignore device_id since that varies per-site.
    builder.AddToken(IdentifiabilityBenignStringToken(device_info->kind()));
    builder.AddToken(IdentifiabilityBenignStringToken(device_info->label()));
    // Ignore group_id since that is varies per-site.
  }
  IdentifiabilityMetricBuilder(document->UkmSourceID())
      .AddWebFeature(WebFeature::kIdentifiabilityMediaDevicesEnumerateDevices,
                     builder.GetToken())
      .Record(document->UkmRecorder());
}

}  // namespace

void MediaDevices::DevicesEnumerated(
    ScriptPromiseResolver* resolver,
    const Vector<Vector<WebMediaDeviceInfo>>& enumeration,
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities,
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  if (!enumerate_device_requests_.Contains(resolver))
    return;

  RequestMetadata request_metadata = enumerate_device_requests_.at(resolver);

  enumerate_device_requests_.erase(resolver);

  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  DCHECK_EQ(static_cast<wtf_size_t>(
                mojom::blink::MediaDeviceType::NUM_MEDIA_DEVICE_TYPES),
            enumeration.size());

  if (!video_input_capabilities.empty()) {
    DCHECK_EQ(enumeration[static_cast<wtf_size_t>(
                              mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT)]
                  .size(),
              video_input_capabilities.size());
  }
  if (!audio_input_capabilities.empty()) {
    DCHECK_EQ(enumeration[static_cast<wtf_size_t>(
                              mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT)]
                  .size(),
              audio_input_capabilities.size());
  }

  MediaDeviceInfoVector media_devices;
  for (wtf_size_t i = 0;
       i < static_cast<wtf_size_t>(
               mojom::blink::MediaDeviceType::NUM_MEDIA_DEVICE_TYPES);
       ++i) {
    for (wtf_size_t j = 0; j < enumeration[i].size(); ++j) {
      mojom::blink::MediaDeviceType device_type =
          static_cast<mojom::blink::MediaDeviceType>(i);
      WebMediaDeviceInfo device_info = enumeration[i][j];
      String device_label = String::FromUTF8(device_info.label);
      if (device_label.Contains("AirPods")) {
        device_label = "AirPods";
      }
      if (device_type == mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT ||
          device_type == mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT) {
        InputDeviceInfo* input_device_info =
            MakeGarbageCollected<InputDeviceInfo>(
                String::FromUTF8(device_info.device_id), device_label,
                String::FromUTF8(device_info.group_id), device_type);
        if (device_type == mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT &&
            !video_input_capabilities.empty()) {
          input_device_info->SetVideoInputCapabilities(
              std::move(video_input_capabilities[j]));
        }
        if (device_type == mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT &&
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

  RecordEnumeratedDevices(resolver, media_devices);
  RecordEnumerateDevicesLatency(request_metadata.start_time);

  if (enumerate_devices_test_callback_)
    std::move(enumerate_devices_test_callback_).Run(media_devices);

  resolver->Resolve(media_devices);
}

void MediaDevices::OnDispatcherHostConnectionError() {
  for (auto& entry : enumerate_device_requests_) {
    ScriptPromiseResolver* resolver = entry.key;
    RequestMetadata& metadata = entry.value;

    RecordEnumerateDevicesLatency(metadata.start_time);
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "enumerateDevices() failed."));
  }
  enumerate_device_requests_.clear();
  dispatcher_host_.reset();

  if (connection_error_test_callback_)
    std::move(connection_error_test_callback_).Run();
}

mojom::blink::MediaDevicesDispatcherHost& MediaDevices::GetDispatcherHost(
    LocalFrame* frame) {
  ExecutionContext* const execution_context = GetExecutionContext();
  DCHECK(execution_context);

  if (!dispatcher_host_.is_bound()) {
    // Note: kInternalMediaRealTime is a better candidate for this job,
    // but kMediaElementEvent is used for consistency.
    frame->GetBrowserInterfaceBroker().GetInterface(
        dispatcher_host_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMediaElementEvent)));
    dispatcher_host_.set_disconnect_handler(
        WTF::BindOnce(&MediaDevices::OnDispatcherHostConnectionError,
                      WrapWeakPersistent(this)));
  }

  DCHECK(dispatcher_host_.get());
  return *dispatcher_host_.get();
}

void MediaDevices::SetDispatcherHostForTesting(
    mojo::PendingRemote<mojom::blink::MediaDevicesDispatcherHost>
        dispatcher_host) {
  ExecutionContext* const execution_context = GetExecutionContext();
  DCHECK(execution_context);

  dispatcher_host_.Bind(
      std::move(dispatcher_host),
      execution_context->GetTaskRunner(TaskType::kMediaElementEvent));
  dispatcher_host_.set_disconnect_handler(
      WTF::BindOnce(&MediaDevices::OnDispatcherHostConnectionError,
                    WrapWeakPersistent(this)));
}

void MediaDevices::Trace(Visitor* visitor) const {
  visitor->Trace(dispatcher_host_);
  visitor->Trace(receiver_);
  visitor->Trace(scheduled_events_);
  visitor->Trace(enumerate_device_requests_);
#if !BUILDFLAG(IS_ANDROID)
  visitor->Trace(crop_id_resolvers_);
#endif
  Supplement<Navigator>::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

#if !BUILDFLAG(IS_ANDROID)
void MediaDevices::EnqueueMicrotaskToCloseFocusWindowOfOpportunity(
    const String& id,
    MediaStreamTrack* track) {
  ExecutionContext* const context = GetExecutionContext();
  if (!context)
    return;

  context->GetAgent()->event_loop()->EnqueueMicrotask(
      WTF::BindOnce(&MediaDevices::CloseFocusWindowOfOpportunity,
                    WrapWeakPersistent(this), id, WrapWeakPersistent(track)));
}

void MediaDevices::CloseFocusWindowOfOpportunity(const String& id,
                                                 MediaStreamTrack* track) {
  if (!track) {
    return;
  }

  ExecutionContext* const context = GetExecutionContext();
  if (!context) {
    return;  // Note: We're still back by the browser-side timer.
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(context);
  if (!window) {
    return;
  }

  // Inform the track that further calls to focus() should raise an exception.
  track->CloseFocusWindowOfOpportunity();

  GetDispatcherHost(window->GetFrame()).CloseFocusWindowOfOpportunity(id);
}

// An empty |crop_id| signals failure; anything else has to be a valid GUID
// and signals success.
void MediaDevices::ResolveProduceCropIdPromise(Element* element,
                                               const WTF::String& crop_id) {
  DCHECK(IsMainThread());
  DCHECK(element);  // Persistent.

  const auto it = crop_id_resolvers_.find(element);
  DCHECK_NE(it, crop_id_resolvers_.end());
  ScriptPromiseResolver* const resolver = it->value;
  crop_id_resolvers_.erase(it);

  if (crop_id.empty()) {
    resolver->Reject();
    RecordUma(ProduceCropTargetPromiseResult::kPromiseRejected);
  } else {
    const base::GUID guid = base::GUID::ParseLowercase(crop_id.Ascii());
    DCHECK(guid.is_valid());
    element->SetRegionCaptureCropId(
        std::make_unique<RegionCaptureCropId>(blink::GUIDToToken(guid)));
    resolver->Resolve(MakeGarbageCollected<CropTarget>(crop_id));
    RecordUma(ProduceCropTargetPromiseResult::kPromiseResolved);
  }
}
#endif

}  // namespace blink
