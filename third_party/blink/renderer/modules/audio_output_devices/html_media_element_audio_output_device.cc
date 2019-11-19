// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/audio_output_devices/html_media_element_audio_output_device.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_set_sink_id_callbacks.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
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
    default:
      NOTREACHED();
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                "Invalid error code");
  }
}

class SetSinkIdResolver : public ScriptPromiseResolver {
 public:
  static SetSinkIdResolver* Create(ScriptState*,
                                   HTMLMediaElement&,
                                   const String& sink_id);
  SetSinkIdResolver(ScriptState*, HTMLMediaElement&, const String& sink_id);
  ~SetSinkIdResolver() override = default;
  void StartAsync();

  void Trace(blink::Visitor*) override;

 private:
  void DoSetSinkId();

  void OnSetSinkIdComplete(base::Optional<WebSetSinkIdError> error);

  Member<HTMLMediaElement> element_;
  String sink_id_;

  DISALLOW_COPY_AND_ASSIGN(SetSinkIdResolver);
};

SetSinkIdResolver* SetSinkIdResolver::Create(ScriptState* script_state,
                                             HTMLMediaElement& element,
                                             const String& sink_id) {
  SetSinkIdResolver* resolver =
      MakeGarbageCollected<SetSinkIdResolver>(script_state, element, sink_id);
  resolver->KeepAliveWhilePending();
  return resolver;
}

SetSinkIdResolver::SetSinkIdResolver(ScriptState* script_state,
                                     HTMLMediaElement& element,
                                     const String& sink_id)
    : ScriptPromiseResolver(script_state),
      element_(element),
      sink_id_(sink_id) {}

void SetSinkIdResolver::StartAsync() {
  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return;
  context->GetTaskRunner(TaskType::kInternalMedia)
      ->PostTask(FROM_HERE, WTF::Bind(&SetSinkIdResolver::DoSetSinkId,
                                      WrapWeakPersistent(this)));
}

void SetSinkIdResolver::DoSetSinkId() {
  auto set_sink_id_completion_callback =
      WTF::Bind(&SetSinkIdResolver::OnSetSinkIdComplete, WrapPersistent(this));
  WebMediaPlayer* web_media_player = element_->GetWebMediaPlayer();
  if (web_media_player) {
    web_media_player->SetSinkId(sink_id_,
                                std::move(set_sink_id_completion_callback));
    return;
  }

  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    // Detached contexts shouldn't be playing audio. Note that despite this
    // explicit Reject(), any associated JS callbacks will never be called
    // because the context is already detached...
    Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        "Impossible to authorize device for detached context"));
    return;
  }

  // This is associated with an HTML element, so the context must be a Document.
  auto& document = To<Document>(*context);
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document.GetFrame());
  if (web_frame && web_frame->Client()) {
    web_frame->Client()->CheckIfAudioSinkExistsAndIsAuthorized(
        sink_id_, std::move(set_sink_id_completion_callback));
  } else {
    Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        "Impossible to authorize device if there is no frame"));
    return;
  }
}

void SetSinkIdResolver::OnSetSinkIdComplete(
    base::Optional<WebSetSinkIdError> error) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;

  if (error) {
    Reject(ToException(*error));
    return;
  }

  HTMLMediaElementAudioOutputDevice& aod_element =
      HTMLMediaElementAudioOutputDevice::From(*element_);
  aod_element.setSinkId(sink_id_);
  Resolve();
}

void SetSinkIdResolver::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  ScriptPromiseResolver::Trace(visitor);
}

}  // namespace

HTMLMediaElementAudioOutputDevice::HTMLMediaElementAudioOutputDevice() {}

String HTMLMediaElementAudioOutputDevice::sinkId(HTMLMediaElement& element) {
  HTMLMediaElementAudioOutputDevice& aod_element =
      HTMLMediaElementAudioOutputDevice::From(element);
  return aod_element.sink_id_;
}

void HTMLMediaElementAudioOutputDevice::setSinkId(const String& sink_id) {
  sink_id_ = sink_id;
}

ScriptPromise HTMLMediaElementAudioOutputDevice::setSinkId(
    ScriptState* script_state,
    HTMLMediaElement& element,
    const String& sink_id) {
  SetSinkIdResolver* resolver =
      SetSinkIdResolver::Create(script_state, element, sink_id);
  ScriptPromise promise = resolver->Promise();
  if (sink_id == HTMLMediaElementAudioOutputDevice::sinkId(element))
    resolver->Resolve();
  else
    resolver->StartAsync();

  return promise;
}

const char HTMLMediaElementAudioOutputDevice::kSupplementName[] =
    "HTMLMediaElementAudioOutputDevice";

HTMLMediaElementAudioOutputDevice& HTMLMediaElementAudioOutputDevice::From(
    HTMLMediaElement& element) {
  HTMLMediaElementAudioOutputDevice* supplement =
      Supplement<HTMLMediaElement>::From<HTMLMediaElementAudioOutputDevice>(
          element);
  if (!supplement) {
    supplement = MakeGarbageCollected<HTMLMediaElementAudioOutputDevice>();
    ProvideTo(element, supplement);
  }
  return *supplement;
}

void HTMLMediaElementAudioOutputDevice::Trace(blink::Visitor* visitor) {
  Supplement<HTMLMediaElement>::Trace(visitor);
}

}  // namespace blink
