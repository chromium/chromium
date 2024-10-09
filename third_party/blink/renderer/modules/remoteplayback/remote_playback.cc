// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"

#include <memory>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "media/base/remoting_constants.h"
#include "third_party/blink/public/platform/modules/remoteplayback/remote_playback_source.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_playback_availability_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_playback_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/remote_playback_observer.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/probe/async_task_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/modules/remoteplayback/availability_callback_wrapper.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback_metrics.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

namespace {

V8RemotePlaybackState::Enum RemotePlaybackStateToEnum(
    mojom::blink::PresentationConnectionState state) {
  switch (state) {
    case mojom::blink::PresentationConnectionState::CONNECTING:
      return V8RemotePlaybackState::Enum::kConnecting;
    case mojom::blink::PresentationConnectionState::CONNECTED:
      return V8RemotePlaybackState::Enum::kConnected;
    case mojom::blink::PresentationConnectionState::CLOSED:
    case mojom::blink::PresentationConnectionState::TERMINATED:
      return V8RemotePlaybackState::Enum::kDisconnected;
  }
  NOTREACHED();
}

void RunRemotePlaybackTask(
    ExecutionContext* context,
    base::OnceClosure task,
    std::unique_ptr<probe::AsyncTaskContext> task_context) {
  probe::AsyncTask async_task(context, task_context.get());
  std::move(task).Run();
}

KURL GetAvailabilityUrl(const WebURL& source,
                        bool is_source_supported,
                        std::optional<media::VideoCodec> video_codec,
                        std::optional<media::AudioCodec> audio_codec) {
  if (source.IsEmpty() || !source.IsValid() || !is_source_supported) {
    return KURL();
  }

  // The URL for each media element's source looks like the following:
  // remote-playback:media-element?source=<encoded-data>&video_codec=<video_codec>&audio_codec=<audio_codec>
  // where |encoded-data| is base64 URL encoded string representation of the
  // source URL. |video_codec| and |audio_codec| are used for device capability
  // filter for Media Remoting based Remote Playback on Desktop. The codec
  // fields are optional.
  std::string source_string = source.GetString().Utf8();
  String encoded_source =
      WTF::Base64URLEncode(base::as_byte_span(source_string));

  std::string video_codec_str =
      video_codec.has_value()
          ? ("&video_codec=" + media::GetCodecName(video_codec.value()))
          : "";
  std::string audio_codec_str =
      audio_codec.has_value()
          ? ("&audio_codec=" + media::GetCodecName(audio_codec.value()))
          : "";
  return KURL(StringView(kRemotePlaybackPresentationUrlPath) +
              "?source=" + encoded_source + video_codec_str.c_str() +
              audio_codec_str.c_str());
}

bool IsBackgroundAvailabilityMonitoringDisabled() {
  return MemoryPressureListenerRegistry::IsLowEndDevice();
}

void RemotingStarting(HTMLMediaElement& media_element) {
  if (auto* video_element = DynamicTo<HTMLVideoElement>(&media_element)) {
    // TODO(xjz): Pass the remote device name.
    video_element->MediaRemotingStarted(WebString());
  }
  media_element.FlingingStarted();
}

}  // anonymous namespace

// static
RemotePlayback& RemotePlayback::From(HTMLMediaElement& element) {
  RemotePlayback* self =
      static_cast<RemotePlayback*>(RemotePlaybackController::From(element));
  if (!self) {
    self = MakeGarbageCollected<RemotePlayback>(element);
    RemotePlaybackController::ProvideTo(element, self);
  }
  return *self;
}

RemotePlayback::RemotePlayback(HTMLMediaElement& element)
    : ExecutionContextLifecycleObserver(element.GetExecutionContext()),
      ActiveScriptWrappable<RemotePlayback>({}),
      RemotePlaybackController(element),
      state_(mojom::blink::PresentationConnectionState::CLOSED),
      availability_(mojom::ScreenAvailability::UNKNOWN),
      media_element_(&element),
      is_listening_(false),
      presentation_connection_receiver_(this, element.GetExecutionContext()),
      target_presentation_connection_(element.GetExecutionContext()) {}

const AtomicString& RemotePlayback::InterfaceName() const {
  return event_target_names::kRemotePlayback;
}

ExecutionContext* RemotePlayback::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

ScriptPromise<IDLLong> RemotePlayback::watchAvailability(
    ScriptState* script_state,
    V8RemotePlaybackAvailabilityCallback* callback,
    ExceptionState& exception_state) {
  if (media_element_->FastHasAttribute(
          html_names::kDisableremoteplaybackAttr)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "disableRemotePlayback attribute is present.");
    return EmptyPromise();
  }

  int id = WatchAvailabilityInternal(
      MakeGarbageCollected<AvailabilityCallbackWrapper>(callback));
  if (id == kWatchAvailabilityNotSupported) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Availability monitoring is not supported on this device.");
    return EmptyPromise();
  }

  // TODO(avayvod): Currently the availability is tracked for each media element
  // as soon as it's created, we probably want to limit that to when the
  // page/element is visible (see https://crbug.com/597281) and has default
  // controls. If there are no default controls, we should also start tracking
  // availability on demand meaning the Promise returned by watchAvailability()
  // will be resolved asynchronously.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLLong>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  resolver->Resolve(id);
  return promise;
}

ScriptPromise<IDLUndefined> RemotePlayback::cancelWatchAvailability(
    ScriptState* script_state,
    int id,
    ExceptionState& exception_state) {
  if (media_element_->FastHasAttribute(
          html_names::kDisableremoteplaybackAttr)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "disableRemotePlayback attribute is present.");
    return EmptyPromise();
  }

  if (!CancelWatchAvailabilityInternal(id)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "A callback with the given id is not found.");
    return EmptyPromise();
  }

  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> RemotePlayback::cancelWatchAvailability(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (media_element_->FastHasAttribute(
          html_names::kDisableremoteplaybackAttr)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "disableRemotePlayback attribute is present.");
    return EmptyPromise();
  }

  availability_callbacks_.clear();
  StopListeningForAvailability();
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> RemotePlayback::prompt(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (media_element_->FastHasAttribute(
          html_names::kDisableremoteplaybackAttr)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "disableRemotePlayback attribute is present.");
    return EmptyPromise();
  }

  if (prompt_promise_resolver_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "A prompt is already being shown for this media element.");
    return EmptyPromise();
  }

  if (!media_element_->DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "RemotePlayback::prompt() does not work in a detached window.");
    return EmptyPromise();
  }

  if (!LocalFrame::HasTransientUserActivation(
          media_element_->DomWindow()->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "RemotePlayback::prompt() requires user gesture.");
    return EmptyPromise();
  }

  if (!RuntimeEnabledFeatures::RemotePlaybackBackendEnabled()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The RemotePlayback API is disabled on this platform.");
    return EmptyPromise();
  }

  if (availability_ == mojom::ScreenAvailability::UNAVAILABLE) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "No remote playback devices found.");
    return EmptyPromise();
  }

  if (availability_ == mojom::ScreenAvailability::SOURCE_NOT_SUPPORTED) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The currentSrc is not compatible with remote playback");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  prompt_promise_resolver_ = resolver;
  PromptInternal();
  RemotePlaybackMetrics::RecordRemotePlaybackLocation(
      RemotePlaybackInitiationLocation::kRemovePlaybackAPI);
  return promise;
}

V8RemotePlaybackState RemotePlayback::state() const {
  return V8RemotePlaybackState(RemotePlaybackStateToEnum(state_));
}

bool RemotePlayback::HasPendingActivity() const {
  return HasEventListeners() || !availability_callbacks_.empty() ||
         prompt_promise_resolver_;
}

void RemotePlayback::PromptInternal() {
  if (!GetExecutionContext())
    return;

  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (controller && !availability_urls_.empty()) {
    controller->GetPresentationService()->StartPresentation(
        availability_urls_,
        WTF::BindOnce(&RemotePlayback::HandlePresentationResponse,
                      WrapPersistent(this)));
  } else {
    // TODO(yuryu): Wrapping PromptCancelled with base::OnceClosure as
    // InspectorInstrumentation requires a globally unique pointer to track
    // tasks. We can remove the wrapper if InspectorInstrumentation returns a
    // task id.
    base::OnceClosure task =
        WTF::BindOnce(&RemotePlayback::PromptCancelled, WrapPersistent(this));

    std::unique_ptr<probe::AsyncTaskContext> task_context =
        std::make_unique<probe::AsyncTaskContext>();
    task_context->Schedule(GetExecutionContext(), "promptCancelled");
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kMediaElementEvent)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(RunRemotePlaybackTask,
                                 WrapPersistent(GetExecutionContext()),
                                 std::move(task), std::move(task_context)));
  }
}

int RemotePlayback::WatchAvailabilityInternal(
    AvailabilityCallbackWrapper* callback) {
  if (RuntimeEnabledFeatures::RemotePlaybackBackendEnabled() &&
      IsBackgroundAvailabilityMonitoringDisabled()) {
    return kWatchAvailabilityNotSupported;
  }

  if (!GetExecutionContext())
    return kWatchAvailabilityNotSupported;

  int id;
  do {
    id = GetExecutionContext()->CircularSequentialID();
  } while (!availability_callbacks_.insert(id, callback).is_new_entry);

  // Report the current availability via the callback.
  // TODO(yuryu): Wrapping notifyInitialAvailability with base::OnceClosure as
  // InspectorInstrumentation requires a globally unique pointer to track tasks.
  // We can remove the wrapper if InspectorInstrumentation returns a task id.
  base::OnceClosure task = WTF::BindOnce(
      &RemotePlayback::NotifyInitialAvailability, WrapPersistent(this), id);
  std::unique_ptr<probe::AsyncTaskContext> task_context =
      std::make_unique<probe::AsyncTaskContext>();
  task_context->Schedule(GetExecutionContext(), "watchAvailabilityCallback");
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMediaElementEvent)
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(RunRemotePlaybackTask,
                               WrapPersistent(GetExecutionContext()),
                               std::move(task), std::move(task_context)));

  MaybeStartListeningForAvailability();
  return id;
}

bool RemotePlayback::CancelWatchAvailabilityInternal(int id) {
  if (id <= 0)  // HashMap doesn't support the cases of key = 0 or key = -1.
    return false;
  auto iter = availability_callbacks_.find(id);
  if (iter == availability_callbacks_.end())
    return false;
  availability_callbacks_.erase(iter);
  if (availability_callbacks_.empty())
    StopListeningForAvailability();

  return true;
}

void RemotePlayback::NotifyInitialAvailability(int callback_id) {
  // May not find the callback if the website cancels it fast enough.
  auto iter = availability_callbacks_.find(callback_id);
  if (iter == availability_callbacks_.end())
    return;

  iter->value->Run(this, RemotePlaybackAvailable());
}

void RemotePlayback::StateChanged(
    mojom::blink::PresentationConnectionState state) {
  if (prompt_promise_resolver_ &&
      IsInParallelAlgorithmRunnable(
          prompt_promise_resolver_->GetExecutionContext(),
          prompt_promise_resolver_->GetScriptState())) {
    // Changing state to "CLOSED" from "CLOSED" or "CONNECTING"
    // means that establishing connection with remote playback device failed.
    // Changing state to anything else means the state change intended by
    // prompt() succeeded.
    ScriptState::Scope script_state_scope(
        prompt_promise_resolver_->GetScriptState());

    if (state_ != mojom::blink::PresentationConnectionState::CONNECTED &&
        state == mojom::blink::PresentationConnectionState::CLOSED) {
      prompt_promise_resolver_->Reject(V8ThrowDOMException::CreateOrDie(
          prompt_promise_resolver_->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kAbortError,
          "Failed to connect to the remote device."));
    } else {
      prompt_promise_resolver_->Resolve();
    }
  }
  prompt_promise_resolver_ = nullptr;

  if (state_ == state)
    return;

  state_ = state;
  if (state_ == mojom::blink::PresentationConnectionState::CONNECTING) {
    DispatchEvent(*Event::Create(event_type_names::kConnecting));
    RemotingStarting(*media_element_);
  } else if (state_ == mojom::blink::PresentationConnectionState::CONNECTED) {
    DispatchEvent(*Event::Create(event_type_names::kConnect));
  } else if (state_ == mojom::blink::PresentationConnectionState::CLOSED ||
             state_ == mojom::blink::PresentationConnectionState::TERMINATED) {
    DispatchEvent(*Event::Create(event_type_names::kDisconnect));
    if (auto* video_element =
            DynamicTo<HTMLVideoElement>(media_element_.Get())) {
      video_element->MediaRemotingStopped(
          WebMediaPlayerClient::kMediaRemotingStopNoText);
    }
    CleanupConnections();
    presentation_id_ = "";
    presentation_url_ = KURL();
    media_element_->FlingingStopped();
  }

  for (auto observer : observers_)
    observer->OnRemotePlaybackStateChanged(state_);
}

void RemotePlayback::PromptCancelled() {
  if (!prompt_promise_resolver_ ||
      !IsInParallelAlgorithmRunnable(
          prompt_promise_resolver_->GetExecutionContext(),
          prompt_promise_resolver_->GetScriptState())) {
    prompt_promise_resolver_ = nullptr;
    return;
  }

  ScriptState::Scope script_state_scope(
      prompt_promise_resolver_->GetScriptState());

  prompt_promise_resolver_->Reject(V8ThrowDOMException::CreateOrDie(
      prompt_promise_resolver_->GetScriptState()->GetIsolate(),
      DOMExceptionCode::kNotAllowedError, "The prompt was dismissed."));
  prompt_promise_resolver_ = nullptr;
}

void RemotePlayback::SourceChanged(const WebURL& source,
                                   bool is_source_supported) {
  source_ = source;
  is_source_supported_ = is_source_supported;

  UpdateAvailabilityUrlsAndStartListening();
}

void RemotePlayback::UpdateAvailabilityUrlsAndStartListening() {
  if (is_background_availability_monitoring_disabled_for_testing_ ||
      IsBackgroundAvailabilityMonitoringDisabled() ||
      !RuntimeEnabledFeatures::RemotePlaybackBackendEnabled()) {
    return;
  }

  // If the video is too short, it's unlikely to be cast. Disable availability
  // monitoring so that the cast buttons are hidden from the video player.
  if (!media_element_ || std::isnan(media_element_->duration()) ||
      media_element_->duration() <=
          media::remoting::kMinRemotingMediaDurationInSec) {
    StopListeningForAvailability();
    availability_urls_.clear();
    return;
  }

  KURL current_url =
      availability_urls_.empty() ? KURL() : availability_urls_[0];
  KURL new_url = GetAvailabilityUrl(source_, is_source_supported_, video_codec_,
                                    audio_codec_);

  if (new_url == current_url)
    return;

  // Tell PresentationController to stop listening for availability before the
  // URLs vector is updated.
  StopListeningForAvailability();

  availability_urls_.clear();
  if (!new_url.IsEmpty()) {
    availability_urls_.push_back(new_url);

    if (state_ == mojom::blink::PresentationConnectionState::CONNECTED) {
      RemotingStarting(*media_element_);
      presentation_url_ = new_url;
    }
  }

  MaybeStartListeningForAvailability();
}

WebString RemotePlayback::GetPresentationId() {
  return presentation_id_;
}

void RemotePlayback::MediaMetadataChanged(
    std::optional<media::VideoCodec> video_codec,
    std::optional<media::AudioCodec> audio_codec) {
  video_codec_ = video_codec;
  audio_codec_ = audio_codec;

  UpdateAvailabilityUrlsAndStartListening();
}

void RemotePlayback::AddObserver(RemotePlaybackObserver* observer) {
  observers_.insert(observer);
}

void RemotePlayback::RemoveObserver(RemotePlaybackObserver* observer) {
  observers_.erase(observer);
}

void RemotePlayback::AvailabilityChangedForTesting(bool screen_is_available) {
  // Disable the background availability monitoring so that the availability
  // won't be overridden later.
  is_background_availability_monitoring_disabled_for_testing_ = true;
  StopListeningForAvailability();

  AvailabilityChanged(screen_is_available
                          ? mojom::blink::ScreenAvailability::AVAILABLE
                          : mojom::blink::ScreenAvailability::UNAVAILABLE);
}

void RemotePlayback::StateChangedForTesting(bool is_connected) {
  StateChanged(is_connected
                   ? mojom::blink::PresentationConnectionState::CONNECTED
                   : mojom::blink::PresentationConnectionState::CLOSED);
}

bool RemotePlayback::RemotePlaybackAvailable() const {
  if (IsBackgroundAvailabilityMonitoringDisabled() &&
      RuntimeEnabledFeatures::RemotePlaybackBackendEnabled() &&
      !media_element_->currentSrc().IsEmpty()) {
    return true;
  }

  return availability_ == mojom::ScreenAvailability::AVAILABLE;
}

void RemotePlayback::RemotePlaybackDisabled() {
  if (prompt_promise_resolver_) {
    prompt_promise_resolver_->Reject(V8ThrowDOMException::CreateOrDie(
        prompt_promise_resolver_->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kInvalidStateError,
        "disableRemotePlayback attribute is present."));
    prompt_promise_resolver_ = nullptr;
  }

  availability_callbacks_.clear();
  StopListeningForAvailability();

  if (state_ == mojom::blink::PresentationConnectionState::CLOSED ||
      state_ == mojom::blink::PresentationConnectionState::TERMINATED) {
    return;
  }

  auto* controller = PresentationController::FromContext(GetExecutionContext());
  if (controller) {
    controller->GetPresentationService()->CloseConnection(presentation_url_,
                                                          presentation_id_);
  }
}

void RemotePlayback::CleanupConnections() {
  target_presentation_connection_.reset();
  presentation_connection_receiver_.reset();
}

void RemotePlayback::AvailabilityChanged(
    mojom::blink::ScreenAvailability availability) {
  DCHECK(is_listening_ ||
         is_background_availability_monitoring_disabled_for_testing_);
  DCHECK_NE(availability, mojom::ScreenAvailability::UNKNOWN);
  DCHECK_NE(availability, mojom::ScreenAvailability::DISABLED);

  if (availability_ == availability)
    return;

  bool old_availability = RemotePlaybackAvailable();
  availability_ = availability;
  bool new_availability = RemotePlaybackAvailable();
  if (new_availability == old_availability)
    return;

  // Copy the callbacks to a temporary vector to prevent iterator invalidations,
  // in case the JS callbacks invoke watchAvailability().
  HeapVector<Member<AvailabilityCallbackWrapper>> callbacks;
  CopyValuesToVector(availability_callbacks_, callbacks);

  for (auto& callback : callbacks)
    callback->Run(this, new_availability);
}

const Vector<KURL>& RemotePlayback::Urls() const {
  // TODO(avayvod): update the URL format and add frame url, mime type and
  // response headers when available.
  return availability_urls_;
}

void RemotePlayback::OnConnectionSuccess(
    mojom::blink::PresentationConnectionResultPtr result) {
  presentation_id_ = std::move(result->presentation_info->id);
  presentation_url_ = std::move(result->presentation_info->url);

  StateChanged(mojom::blink::PresentationConnectionState::CONNECTING);

  DCHECK(!presentation_connection_receiver_.is_bound());
  auto* presentation_controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!presentation_controller)
    return;

#if !BUILDFLAG(IS_ANDROID)
  media_element_->Play();
  media_element_->GetWebMediaPlayer()->RequestMediaRemoting();
#endif

  // Note: Messages on |connection_receiver| are ignored.
  target_presentation_connection_.Bind(
      std::move(result->connection_remote),
      GetExecutionContext()->GetTaskRunner(TaskType::kMediaElementEvent));
  presentation_connection_receiver_.Bind(
      std::move(result->connection_receiver),
      GetExecutionContext()->GetTaskRunner(TaskType::kMediaElementEvent));
  RemotePlaybackMetrics::RecordRemotePlaybackStartSessionResult(
      GetExecutionContext(), true);
}

void RemotePlayback::OnConnectionError(
    const mojom::blink::PresentationError& error) {
  // This is called when:
  // (1) A request to start a presentation failed.
  // (2) A PresentationRequest is cancelled. i.e. the user closed the device
  // selection or the route controller dialog.

  if (error.error_type ==
      mojom::blink::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED) {
    PromptCancelled();
    return;
  }

  presentation_id_ = "";
  presentation_url_ = KURL();

  StateChanged(mojom::blink::PresentationConnectionState::CLOSED);
  RemotePlaybackMetrics::RecordRemotePlaybackStartSessionResult(
      GetExecutionContext(), false);
}

void RemotePlayback::HandlePresentationResponse(
    mojom::blink::PresentationConnectionResultPtr result,
    mojom::blink::PresentationErrorPtr error) {
  if (result) {
    OnConnectionSuccess(std::move(result));
  } else {
    OnConnectionError(*error);
  }
}

void RemotePlayback::OnMessage(
    mojom::blink::PresentationConnectionMessagePtr message) {
  // Messages are ignored.
}

void RemotePlayback::DidChangeState(
    mojom::blink::PresentationConnectionState state) {
  StateChanged(state);
}

void RemotePlayback::DidClose(
    mojom::blink::PresentationConnectionCloseReason reason) {
  StateChanged(mojom::blink::PresentationConnectionState::CLOSED);
}

void RemotePlayback::StopListeningForAvailability() {
  if (!is_listening_)
    return;

  availability_ = mojom::ScreenAvailability::UNKNOWN;
  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller)
    return;

  controller->RemoveAvailabilityObserver(this);
  is_listening_ = false;
}

void RemotePlayback::MaybeStartListeningForAvailability() {
  if (IsBackgroundAvailabilityMonitoringDisabled() ||
      is_background_availability_monitoring_disabled_for_testing_) {
    return;
  }

  if (is_listening_)
    return;

  if (availability_urls_.empty() || availability_callbacks_.empty())
    return;

  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller)
    return;

  controller->AddAvailabilityObserver(this);
  is_listening_ = true;
}

void RemotePlayback::Trace(Visitor* visitor) const {
  visitor->Trace(availability_callbacks_);
  visitor->Trace(prompt_promise_resolver_);
  visitor->Trace(media_element_);
  visitor->Trace(presentation_connection_receiver_);
  visitor->Trace(target_presentation_connection_);
  visitor->Trace(observers_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  RemotePlaybackController::Trace(visitor);
}

}  // namespace blink
