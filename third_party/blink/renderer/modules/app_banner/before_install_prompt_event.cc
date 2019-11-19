// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/app_banner/before_install_prompt_event.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/app_banner/before_install_prompt_event_init.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

BeforeInstallPromptEvent::BeforeInstallPromptEvent(
    const AtomicString& name,
    LocalFrame& frame,
    mojo::PendingRemote<mojom::blink::AppBannerService> service_remote,
    mojo::PendingReceiver<mojom::blink::AppBannerEvent> event_receiver,
    const Vector<String>& platforms)
    : Event(name, Bubbles::kNo, Cancelable::kYes),
      ContextClient(&frame),
      banner_service_remote_(std::move(service_remote)),
      receiver_(this,
                std::move(event_receiver),
                frame.GetTaskRunner(TaskType::kApplicationLifeCycle)),
      platforms_(platforms),
      user_choice_(MakeGarbageCollected<UserChoiceProperty>(
          frame.GetDocument(),
          this,
          UserChoiceProperty::kUserChoice)) {
  DCHECK(banner_service_remote_);
  DCHECK(receiver_.is_bound());
  UseCounter::Count(frame.GetDocument(), WebFeature::kBeforeInstallPromptEvent);
}

BeforeInstallPromptEvent::BeforeInstallPromptEvent(
    ExecutionContext* execution_context,
    const AtomicString& name,
    const BeforeInstallPromptEventInit* init)
    : Event(name, init), ContextClient(execution_context) {
  if (init->hasPlatforms())
    platforms_ = init->platforms();
}

BeforeInstallPromptEvent::~BeforeInstallPromptEvent() = default;

void BeforeInstallPromptEvent::Dispose() {
  banner_service_remote_.reset();
  receiver_.reset();
}

Vector<String> BeforeInstallPromptEvent::platforms() const {
  return platforms_;
}

ScriptPromise BeforeInstallPromptEvent::userChoice(ScriptState* script_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kBeforeInstallPromptEventUserChoice);
  // |m_binding| must be bound to allow the AppBannerService to resolve the
  // userChoice promise.
  if (user_choice_ && receiver_.is_bound())
    return user_choice_->Promise(script_state->World());
  return ScriptPromise::RejectWithDOMException(
      script_state, MakeGarbageCollected<DOMException>(
                        DOMExceptionCode::kInvalidStateError,
                        "userChoice cannot be accessed on this event."));
}

ScriptPromise BeforeInstallPromptEvent::prompt(ScriptState* script_state) {
  // |m_bannerService| must be bound to allow us to inform the AppBannerService
  // to display the banner now.
  if (!banner_service_remote_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "The prompt() method cannot be called."));
  }

  ExecutionContext* context = ExecutionContext::From(script_state);
  Document* doc = To<Document>(context);

  if (!LocalFrame::ConsumeTransientUserActivation(doc ? doc->GetFrame()
                                                      : nullptr)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "The prompt() method must be called with a user gesture"));
  }

  UseCounter::Count(context, WebFeature::kBeforeInstallPromptEventPrompt);
  banner_service_remote_->DisplayAppBanner();
  return user_choice_->Promise(script_state->World());
}

const AtomicString& BeforeInstallPromptEvent::InterfaceName() const {
  return event_interface_names::kBeforeInstallPromptEvent;
}

void BeforeInstallPromptEvent::preventDefault() {
  Event::preventDefault();
  if (target()) {
    UseCounter::Count(target()->GetExecutionContext(),
                      WebFeature::kBeforeInstallPromptEventPreventDefault);
  }
}

bool BeforeInstallPromptEvent::HasPendingActivity() const {
  return user_choice_ &&
         user_choice_->GetState() == ScriptPromisePropertyBase::kPending;
}

void BeforeInstallPromptEvent::BannerAccepted(const String& platform) {
  AppBannerPromptResult* result = AppBannerPromptResult::Create();
  result->setPlatform(platform);
  result->setOutcome("accepted");
  user_choice_->Resolve(result);
}

void BeforeInstallPromptEvent::BannerDismissed() {
  AppBannerPromptResult* result = AppBannerPromptResult::Create();
  result->setPlatform(g_empty_atom);
  result->setOutcome("dismissed");
  user_choice_->Resolve(result);
}

void BeforeInstallPromptEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(user_choice_);
  Event::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
