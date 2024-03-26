// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/audio_output_devices/html_media_element_audio_output_device.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_set_sink_id_callbacks.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

DOMException* ToException(WebSetSinkIdError error) {
  switch (error) {
    case WebSetSinkIdError::kNotFound:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, "Requested device not found");
    case WebSetSinkIdError::kNotAuthorized:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "No permission to use requested device");
    case WebSetSinkIdError::kAborted:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "The operation could not be performed and was aborted");
    case WebSetSinkIdError::kNotSupported:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Operation not supported");
  }
}

class SetSinkIdResolver : public GarbageCollected<SetSinkIdResolver> {
 public:
  SetSinkIdResolver(ScriptPromiseResolver<IDLUndefined>*,
                    HTMLMediaElement&,
                    const String& sink_id);

  SetSinkIdResolver(const SetSinkIdResolver&) = delete;
  SetSinkIdResolver& operator=(const SetSinkIdResolver&) = delete;
  ~SetSinkIdResolver() = default;

  void StartAsync();
  void Start();
  void Trace(Visitor*) const;

 private:
  void DoSetSinkId();

  void OnSetSinkIdComplete(std::optional<WebSetSinkIdError> error);

  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  Member<HTMLMediaElement> element_;
  String sink_id_;
};

SetSinkIdResolver::SetSinkIdResolver(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    HTMLMediaElement& element,
    const String& sink_id)
    : resolver_(resolver), element_(element), sink_id_(sink_id) {}

void SetSinkIdResolver::StartAsync() {
  ExecutionContext* context = element_->GetExecutionContext();
  if (!context)
    return;
  context->GetTaskRunner(TaskType::kInternalMedia)
      ->PostTask(FROM_HERE, WTF::BindOnce(&SetSinkIdResolver::DoSetSinkId,
                                          WrapPersistent(this)));
}

void SetSinkIdResolver::Start() {
  auto* context = element_->GetExecutionContext();
  if (!context || context->IsContextDestroyed())
    return;

  if (LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context)) {
    if (window->document()->IsPrerendering()) {
      window->document()->AddPostPrerenderingActivationStep(
          WTF::BindOnce(&SetSinkIdResolver::Start, WrapPersistent(this)));
      return;
    }
  }

  // Validate that sink_id_ is a valid UTF8 - see https://crbug.com/1420170.
  if (sink_id_.Utf8(WTF::kStrictUTF8Conversion).empty() != sink_id_.empty()) {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidCharacterError, "Invalid sink id."));
    return;
  }

  if (sink_id_ == HTMLMediaElementAudioOutputDevice::sinkId(*element_))
    resolver_->Resolve();
  else
    StartAsync();
}

void SetSinkIdResolver::DoSetSinkId() {
  auto set_sink_id_completion_callback = WTF::BindOnce(
      &SetSinkIdResolver::OnSetSinkIdComplete, WrapPersistent(this));
  WebMediaPlayer* web_media_player = element_->GetWebMediaPlayer();
  if (web_media_player) {
    if (web_media_player->SetSinkId(
            sink_id_, std::move(set_sink_id_completion_callback))) {
      element_->DidAudioOutputSinkChanged(sink_id_);
    }
    return;
  }

  ExecutionContext* context = element_->GetExecutionContext();
  if (!context) {
    return;
  }

  // This is associated with an HTML element, so the context must be a window.
  if (WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(
          To<LocalDOMWindow>(context)->GetFrame())) {
    web_frame->Client()->CheckIfAudioSinkExistsAndIsAuthorized(
        sink_id_, std::move(set_sink_id_completion_callback));
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        "Impossible to authorize device if there is no frame"));
  }
}

void SetSinkIdResolver::OnSetSinkIdComplete(
    std::optional<WebSetSinkIdError> error) {
  ExecutionContext* context = element_->GetExecutionContext();
  if (!context || context->IsContextDestroyed()) {
    return;
  }

  if (error) {
    resolver_->Reject(ToException(*error));
    return;
  }

  HTMLMediaElementAudioOutputDevice& aod_element =
      HTMLMediaElementAudioOutputDevice::From(*element_);
  aod_element.setSinkId(sink_id_);
  resolver_->Resolve();
}

void SetSinkIdResolver::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(resolver_);
}

}  // namespace

HTMLMediaElementAudioOutputDevice::HTMLMediaElementAudioOutputDevice(
    HTMLMediaElement& element)
    : AudioOutputDeviceController(element) {}

// static
HTMLMediaElementAudioOutputDevice& HTMLMediaElementAudioOutputDevice::From(
    HTMLMediaElement& element) {
  HTMLMediaElementAudioOutputDevice* self =
      static_cast<HTMLMediaElementAudioOutputDevice*>(
          AudioOutputDeviceController::From(element));
  if (!self) {
    self = MakeGarbageCollected<HTMLMediaElementAudioOutputDevice>(element);
    AudioOutputDeviceController::ProvideTo(element, self);
  }
  return *self;
}

String HTMLMediaElementAudioOutputDevice::sinkId(HTMLMediaElement& element) {
  HTMLMediaElementAudioOutputDevice& aod_element =
      HTMLMediaElementAudioOutputDevice::From(element);
  return aod_element.sink_id_;
}

void HTMLMediaElementAudioOutputDevice::setSinkId(const String& sink_id) {
  sink_id_ = sink_id;
}

ScriptPromise<IDLUndefined> HTMLMediaElementAudioOutputDevice::setSinkId(
    ScriptState* script_state,
    HTMLMediaElement& element,
    const String& sink_id) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  MakeGarbageCollected<SetSinkIdResolver>(resolver, element, sink_id)->Start();
  return promise;
}

void HTMLMediaElementAudioOutputDevice::SetSinkId(const String& sink_id) {
  // No need to call WebFrameClient::CheckIfAudioSinkExistsAndIsAuthorized as
  // this call is not coming from content and should already be allowed.
  HTMLMediaElement* html_media_element = GetSupplementable();
  WebMediaPlayer* web_media_player = html_media_element->GetWebMediaPlayer();
  if (!web_media_player)
    return;

  sink_id_ = sink_id;

  if (web_media_player->SetSinkId(sink_id_, base::DoNothing()))
    html_media_element->DidAudioOutputSinkChanged(sink_id_);
}

void HTMLMediaElementAudioOutputDevice::Trace(Visitor* visitor) const {
  AudioOutputDeviceController::Trace(visitor);
}

}  // namespace blink
