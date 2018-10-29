// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_playback_availability_callback.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/modules/remoteplayback/availability_callback_wrapper.h"
#include "third_party/blink/renderer/platform/memory_coordinator.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

namespace {

const AtomicString& RemotePlaybackStateToString(WebRemotePlaybackState state) {
  DEFINE_STATIC_LOCAL(const AtomicString, connecting_value, ("connecting"));
  DEFINE_STATIC_LOCAL(const AtomicString, connected_value, ("connected"));
  DEFINE_STATIC_LOCAL(const AtomicString, disconnected_value, ("disconnected"));

  switch (state) {
    case WebRemotePlaybackState::kConnecting:
      return connecting_value;
    case WebRemotePlaybackState::kConnected:
      return connected_value;
    case WebRemotePlaybackState::kDisconnected:
      return disconnected_value;
  }

  NOTREACHED();
  return disconnected_value;
}

void RunRemotePlaybackTask(ExecutionContext* context,
                           base::OnceClosure task,
                           std::unique_ptr<int> task_id) {
  probe::AsyncTask async_task(context, task_id.get());
  std::move(task).Run();
}

KURL GetAvailabilityUrl(const WebURL& source, bool is_source_supported) {
  if (source.IsEmpty() || !source.IsValid() || !is_source_supported)
    return KURL();

  // The URL for each media element's source looks like the following:
  // remote-playback://<encoded-data> where |encoded-data| is base64 URL
  // encoded string representation of the source URL.
  std::string source_string = source.GetString().Utf8();
  String encoded_source = WTF::Base64URLEncode(
      source_string.data(), SafeCast<unsigned>(source_string.length()));

  return KURL("remote-playback://" + encoded_source);
}

bool IsBackgroundAvailabilityMonitoringDisabled() {
  return MemoryCoordinator::IsLowEndDevice();
}

}  // anonymous namespace

// static
RemotePlayback* RemotePlayback::Create(HTMLMediaElement& element) {
  return new RemotePlayback(element);
}

RemotePlayback::RemotePlayback(HTMLMediaElement& element)
    : ContextLifecycleObserver(element.GetExecutionContext()),
      state_(element.IsPlayingRemotely()
                 ? WebRemotePlaybackState::kConnected
                 : WebRemotePlaybackState::kDisconnected),
      availability_(WebRemotePlaybackAvailability::kUnknown),
      media_element_(&element),
      is_listening_(false),
      presentation_connection_binding_(this) {}

const AtomicString& RemotePlayback::InterfaceName() const {
  return EventTargetNames::RemotePlayback;
}

ExecutionContext* RemotePlayback::GetExecutionContext() const {
  return &media_element_->GetDocument();
}

ScriptPromise RemotePlayback::watchAvailability(
    ScriptState* script_state,
    V8RemotePlaybackAvailabilityCallback* callback) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "disableRemotePlayback attribute is present."));
    return promise;
  }

  int id = WatchAvailabilityInternal(new AvailabilityCallbackWrapper(callback));
  if (id == kWatchAvailabilityNotSupported) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotSupportedError,
        "Availability monitoring is not supported on this device."));
    return promise;
  }

  // TODO(avayvod): Currently the availability is tracked for each media element
  // as soon as it's created, we probably want to limit that to when the
  // page/element is visible (see https://crbug.com/597281) and has default
  // controls. If there are no default controls, we should also start tracking
  // availability on demand meaning the Promise returned by watchAvailability()
  // will be resolved asynchronously.
  resolver->Resolve(id);
  return promise;
}

ScriptPromise RemotePlayback::cancelWatchAvailability(ScriptState* script_state,
                                                      int id) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "disableRemotePlayback attribute is present."));
    return promise;
  }

  if (!CancelWatchAvailabilityInternal(id)) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kNotFoundError,
                             "A callback with the given id is not found."));
    return promise;
  }

  resolver->Resolve();
  return promise;
}

ScriptPromise RemotePlayback::cancelWatchAvailability(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "disableRemotePlayback attribute is present."));
    return promise;
  }

  availability_callbacks_.clear();
  StopListeningForAvailability();

  resolver->Resolve();
  return promise;
}

ScriptPromise RemotePlayback::prompt(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "disableRemotePlayback attribute is present."));
    return promise;
  }

  if (prompt_promise_resolver_) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kOperationError,
        "A prompt is already being shown for this media element."));
    return promise;
  }

  if (!LocalFrame::HasTransientUserActivation(media_element_->GetFrame())) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kInvalidAccessError,
        "RemotePlayback::prompt() requires user gesture."));
    return promise;
  }

  if (!RuntimeEnabledFeatures::RemotePlaybackBackendEnabled()) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotSupportedError,
        "The RemotePlayback API is disabled on this platform."));
    return promise;
  }

  if (availability_ == WebRemotePlaybackAvailability::kDeviceNotAvailable) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          "No remote playback devices found."));
    return promise;
  }

  if (availability_ == WebRemotePlaybackAvailability::kSourceNotCompatible) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotSupportedError,
        "The currentSrc is not compatible with remote playback"));
    return promise;
  }

  prompt_promise_resolver_ = resolver;
  PromptInternal();

  return promise;
}

String RemotePlayback::state() const {
  return RemotePlaybackStateToString(state_);
}

bool RemotePlayback::HasPendingActivity() const {
  return HasEventListeners() || !availability_callbacks_.IsEmpty() ||
         prompt_promise_resolver_;
}

void RemotePlayback::ContextDestroyed(ExecutionContext*) {
  CleanupConnections();
}

void RemotePlayback::PromptInternal() {
  DCHECK(RuntimeEnabledFeatures::RemotePlaybackBackendEnabled());

  if (RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled()) {
    PresentationController* controller =
        PresentationController::FromContext(GetExecutionContext());
    if (controller && !availability_urls_.IsEmpty()) {
      controller->GetPresentationService()->StartPresentation(
          availability_urls_,
          WTF::Bind(&RemotePlayback::HandlePresentationResponse,
                    WrapPersistent(this)));
    } else {
      // TODO(yuryu): Wrapping PromptCancelled with base::OnceClosure as
      // InspectorInstrumentation requires a globally unique pointer to track
      // tasks. We can remove the wrapper if InspectorInstrumentation returns a
      // task id.
      base::OnceClosure task =
          WTF::Bind(&RemotePlayback::PromptCancelled, WrapPersistent(this));
      std::unique_ptr<int> task_id = std::make_unique<int>(0);
      probe::AsyncTaskScheduled(GetExecutionContext(), "promptCancelled",
                                task_id.get());
      GetExecutionContext()
          ->GetTaskRunner(TaskType::kMediaElementEvent)
          ->PostTask(FROM_HERE, WTF::Bind(RunRemotePlaybackTask,
                                          WrapPersistent(GetExecutionContext()),
                                          WTF::Passed(std::move(task)),
                                          WTF::Passed(std::move(task_id))));
    }
    return;
  }

  if (state_ == WebRemotePlaybackState::kDisconnected)
    media_element_->RequestRemotePlayback();
  else
    media_element_->RequestRemotePlaybackControl();
}

int RemotePlayback::WatchAvailabilityInternal(
    AvailabilityCallbackWrapper* callback) {
  if (RuntimeEnabledFeatures::RemotePlaybackBackendEnabled() &&
      IsBackgroundAvailabilityMonitoringDisabled()) {
    return kWatchAvailabilityNotSupported;
  }

  int id;
  do {
    id = GetExecutionContext()->CircularSequentialID();
  } while (!availability_callbacks_.insert(id, callback).is_new_entry);

  // Report the current availability via the callback.
  // TODO(yuryu): Wrapping notifyInitialAvailability with base::OnceClosure as
  // InspectorInstrumentation requires a globally unique pointer to track tasks.
  // We can remove the wrapper if InspectorInstrumentation returns a task id.
  base::OnceClosure task = WTF::Bind(&RemotePlayback::NotifyInitialAvailability,
                                     WrapPersistent(this), id);
  std::unique_ptr<int> task_id = std::make_unique<int>(0);
  probe::AsyncTaskScheduled(GetExecutionContext(), "watchAvailabilityCallback",
                            task_id.get());
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMediaElementEvent)
      ->PostTask(FROM_HERE, WTF::Bind(RunRemotePlaybackTask,
                                      WrapPersistent(GetExecutionContext()),
                                      WTF::Passed(std::move(task)),
                                      WTF::Passed(std::move(task_id))));

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
  if (availability_callbacks_.IsEmpty())
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

void RemotePlayback::StateChanged(WebRemotePlaybackState state) {
  if (prompt_promise_resolver_) {
    // Changing state to "disconnected" from "disconnected" or "connecting"
    // means that establishing connection with remote playback device failed.
    // Changing state to anything else means the state change intended by
    // prompt() succeeded.
    if (state_ != WebRemotePlaybackState::kConnected &&
        state == WebRemotePlaybackState::kDisconnected) {
      prompt_promise_resolver_->Reject(
          DOMException::Create(DOMExceptionCode::kAbortError,
                               "Failed to connect to the remote device."));
    } else {
      DCHECK((state_ == WebRemotePlaybackState::kDisconnected &&
              state == WebRemotePlaybackState::kConnecting) ||
             (state_ == WebRemotePlaybackState::kConnected &&
              state == WebRemotePlaybackState::kDisconnected));
      prompt_promise_resolver_->Resolve();
    }
    prompt_promise_resolver_ = nullptr;
  }

  if (state_ == state)
    return;

  state_ = state;
  switch (state_) {
    case WebRemotePlaybackState::kConnecting:
      DispatchEvent(*Event::Create(EventTypeNames::connecting));
      if (RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled()) {
        if (media_element_->IsHTMLVideoElement()) {
          // TODO(xjz): Pass the remote device name.
          ToHTMLVideoElement(media_element_)->MediaRemotingStarted(WebString());
        }
        media_element_->FlingingStarted();
      }
      break;
    case WebRemotePlaybackState::kConnected:
      DispatchEvent(*Event::Create(EventTypeNames::connect));
      break;
    case WebRemotePlaybackState::kDisconnected:
      DispatchEvent(*Event::Create(EventTypeNames::disconnect));
      if (RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled()) {
        if (media_element_->IsHTMLVideoElement()) {
          ToHTMLVideoElement(media_element_)
              ->MediaRemotingStopped(
                  WebLocalizedString::kMediaRemotingStopNoText);
        }
        CleanupConnections();
        presentation_id_ = "";
        presentation_url_ = KURL();
        media_element_->FlingingStopped();
      }
      break;
  }
}

void RemotePlayback::AvailabilityChanged(
    WebRemotePlaybackAvailability availability) {
  if (availability_ == availability)
    return;

  bool old_availability = RemotePlaybackAvailable();
  availability_ = availability;
  bool new_availability = RemotePlaybackAvailable();
  if (new_availability == old_availability)
    return;

  for (auto& callback : availability_callbacks_.Values())
    callback->Run(this, new_availability);
}

void RemotePlayback::PromptCancelled() {
  if (!prompt_promise_resolver_)
    return;

  prompt_promise_resolver_->Reject(DOMException::Create(
      DOMExceptionCode::kNotAllowedError, "The prompt was dismissed."));
  prompt_promise_resolver_ = nullptr;
}

void RemotePlayback::SourceChanged(const WebURL& source,
                                   bool is_source_supported) {
  DCHECK(RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled());

  if (IsBackgroundAvailabilityMonitoringDisabled())
    return;

  KURL current_url =
      availability_urls_.IsEmpty() ? KURL() : availability_urls_[0];
  KURL new_url = GetAvailabilityUrl(source, is_source_supported);

  if (new_url == current_url)
    return;

  // Tell PresentationController to stop listening for availability before the
  // URLs vector is updated.
  StopListeningForAvailability();

  availability_urls_.clear();
  if (!new_url.IsEmpty())
    availability_urls_.push_back(new_url);

  MaybeStartListeningForAvailability();
}

WebString RemotePlayback::GetPresentationId() {
  return presentation_id_;
}

bool RemotePlayback::RemotePlaybackAvailable() const {
  if (IsBackgroundAvailabilityMonitoringDisabled() &&
      RuntimeEnabledFeatures::RemotePlaybackBackendEnabled() &&
      !media_element_->currentSrc().IsEmpty()) {
    return true;
  }

  return availability_ == WebRemotePlaybackAvailability::kDeviceAvailable;
}

void RemotePlayback::RemotePlaybackDisabled() {
  if (prompt_promise_resolver_) {
    prompt_promise_resolver_->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "disableRemotePlayback attribute is present."));
    prompt_promise_resolver_ = nullptr;
  }

  availability_callbacks_.clear();
  StopListeningForAvailability();

  if (state_ == WebRemotePlaybackState::kDisconnected)
    return;

  if (RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled()) {
    auto* controller =
        PresentationController::FromContext(GetExecutionContext());
    if (controller) {
      controller->GetPresentationService()->CloseConnection(presentation_url_,
                                                            presentation_id_);
    }
  } else {
    media_element_->RequestRemotePlaybackStop();
  }
}

void RemotePlayback::CleanupConnections() {
  target_presentation_connection_.reset();
  presentation_connection_binding_.Close();
}

void RemotePlayback::AvailabilityChanged(
    mojom::blink::ScreenAvailability availability) {
  DCHECK(RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled());
  DCHECK(is_listening_);

  // TODO(avayvod): Use mojom::ScreenAvailability directly once
  // WebRemotePlaybackAvailability is gone with the old pipeline.
  WebRemotePlaybackAvailability remote_playback_availability =
      WebRemotePlaybackAvailability::kUnknown;
  switch (availability) {
    case mojom::ScreenAvailability::UNKNOWN:
    case mojom::ScreenAvailability::DISABLED:
      NOTREACHED();
      remote_playback_availability = WebRemotePlaybackAvailability::kUnknown;
      break;
    case mojom::ScreenAvailability::UNAVAILABLE:
      remote_playback_availability =
          WebRemotePlaybackAvailability::kDeviceNotAvailable;
      break;
    case mojom::ScreenAvailability::SOURCE_NOT_SUPPORTED:
      remote_playback_availability =
          WebRemotePlaybackAvailability::kSourceNotCompatible;
      break;
    case mojom::ScreenAvailability::AVAILABLE:
      remote_playback_availability =
          WebRemotePlaybackAvailability::kDeviceAvailable;
      break;
  }
  AvailabilityChanged(remote_playback_availability);
}

const Vector<KURL>& RemotePlayback::Urls() const {
  DCHECK(RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled());
  // TODO(avayvod): update the URL format and add frame url, mime type and
  // response headers when available.
  return availability_urls_;
}

void RemotePlayback::OnConnectionSuccess(
    mojom::blink::PresentationConnectionResultPtr result) {
  DCHECK(RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled());
  presentation_id_ = std::move(result->presentation_info->id);
  presentation_url_ = std::move(result->presentation_info->url);

  StateChanged(WebRemotePlaybackState::kConnecting);

  DCHECK(!presentation_connection_binding_.is_bound());
  auto* presentation_controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!presentation_controller)
    return;

  // Note: Messages on |connection_request| are ignored.
  target_presentation_connection_.Bind(std::move(result->connection_ptr));
  presentation_connection_binding_.Bind(std::move(result->connection_request));
}

void RemotePlayback::OnConnectionError(
    const mojom::blink::PresentationError& error) {
  DCHECK(RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled());
  presentation_id_ = "";
  presentation_url_ = KURL();
  if (error.error_type ==
      mojom::blink::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED) {
    PromptCancelled();
    return;
  }

  StateChanged(WebRemotePlaybackState::kDisconnected);
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
  WebRemotePlaybackState remote_playback_state =
      WebRemotePlaybackState::kDisconnected;
  if (state == mojom::blink::PresentationConnectionState::CONNECTING)
    remote_playback_state = WebRemotePlaybackState::kConnecting;
  else if (state == mojom::blink::PresentationConnectionState::CONNECTED)
    remote_playback_state = WebRemotePlaybackState::kConnected;

  StateChanged(remote_playback_state);
}

void RemotePlayback::DidClose(
    mojom::blink::PresentationConnectionCloseReason reason) {
  StateChanged(WebRemotePlaybackState::kDisconnected);
}

void RemotePlayback::StopListeningForAvailability() {
  if (!RuntimeEnabledFeatures::RemotePlaybackBackendEnabled())
    return;

  if (!is_listening_)
    return;

  availability_ = WebRemotePlaybackAvailability::kUnknown;
  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller)
    return;

  controller->RemoveAvailabilityObserver(this);
  is_listening_ = false;
}

void RemotePlayback::MaybeStartListeningForAvailability() {
  if (IsBackgroundAvailabilityMonitoringDisabled())
    return;

  if (!RuntimeEnabledFeatures::RemotePlaybackBackendEnabled())
    return;

  if (is_listening_)
    return;

  if (availability_urls_.IsEmpty() || availability_callbacks_.IsEmpty())
    return;

  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller)
    return;

  controller->AddAvailabilityObserver(this);
  is_listening_ = true;
}

void RemotePlayback::Trace(blink::Visitor* visitor) {
  visitor->Trace(availability_callbacks_);
  visitor->Trace(prompt_promise_resolver_);
  visitor->Trace(media_element_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
