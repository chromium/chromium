// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/audio_output_devices/html_media_element_audio_output_device.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/modules/audio_output_devices/set_sink_id_callbacks.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

class SetSinkIdResolver : public ScriptPromiseResolver {
  WTF_MAKE_NONCOPYABLE(SetSinkIdResolver);

 public:
  static SetSinkIdResolver* Create(ScriptState*,
                                   HTMLMediaElement&,
                                   const String& sink_id);
  ~SetSinkIdResolver() override = default;
  void StartAsync();

  void Trace(blink::Visitor*) override;

 private:
  SetSinkIdResolver(ScriptState*, HTMLMediaElement&, const String& sink_id);
  void TimerFired(TimerBase*);

  Member<HTMLMediaElement> element_;
  String sink_id_;
  TaskRunnerTimer<SetSinkIdResolver> timer_;
};

SetSinkIdResolver* SetSinkIdResolver::Create(ScriptState* script_state,
                                             HTMLMediaElement& element,
                                             const String& sink_id) {
  SetSinkIdResolver* resolver =
      new SetSinkIdResolver(script_state, element, sink_id);
  resolver->PauseIfNeeded();
  resolver->KeepAliveWhilePending();
  return resolver;
}

SetSinkIdResolver::SetSinkIdResolver(ScriptState* script_state,
                                     HTMLMediaElement& element,
                                     const String& sink_id)
    : ScriptPromiseResolver(script_state),
      element_(element),
      sink_id_(sink_id),
      timer_(ExecutionContext::From(script_state)
                 ->GetTaskRunner(TaskType::kMiscPlatformAPI),
             this,
             &SetSinkIdResolver::TimerFired) {}

void SetSinkIdResolver::StartAsync() {
  timer_.StartOneShot(TimeDelta(), FROM_HERE);
}

void SetSinkIdResolver::TimerFired(TimerBase* timer) {
  ExecutionContext* context = GetExecutionContext();
  std::unique_ptr<SetSinkIdCallbacks> callbacks =
      std::make_unique<SetSinkIdCallbacks>(this, *element_, sink_id_);
  WebMediaPlayer* web_media_player = element_->GetWebMediaPlayer();
  if (web_media_player) {
    // Using release() to transfer ownership because |webMediaPlayer| is a
    // platform object that takes raw pointers.
    web_media_player->SetSinkId(sink_id_, std::move(callbacks));
    return;
  }

  if (!context) {
    // Detached contexts shouldn't be playing audio. Note that despite this
    // explicit Reject(), any associated JS callbacks will never be called
    // because the context is already detached...
    Reject(DOMException::Create(
        DOMExceptionCode::kSecurityError,
        "Impossible to authorize device for detached context"));
    return;
  }

  // This is associated with an HTML element, so the context must be a Document.
  auto& document = To<Document>(*context);
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document.GetFrame());
  web_frame->Client()->CheckIfAudioSinkExistsAndIsAuthorized(
      sink_id_, std::move(callbacks));
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
    supplement = new HTMLMediaElementAudioOutputDevice();
    ProvideTo(element, supplement);
  }
  return *supplement;
}

void HTMLMediaElementAudioOutputDevice::Trace(blink::Visitor* visitor) {
  Supplement<HTMLMediaElement>::Trace(visitor);
}

}  // namespace blink
