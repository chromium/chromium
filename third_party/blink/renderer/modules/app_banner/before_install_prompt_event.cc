// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/app_banner/before_install_prompt_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_before_install_prompt_event_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

BeforeInstallPromptEvent::BeforeInstallPromptEvent(
    const AtomicString& name,
    ExecutionContext& context,
    mojo::PendingRemote<mojom::blink::AppBannerService> service_remote,
    mojo::PendingReceiver<mojom::blink::AppBannerEvent> event_receiver,
    const Vector<String>& platforms)
    : Event(name, Bubbles::kNo, Cancelable::kYes),
      ActiveScriptWrappable<BeforeInstallPromptEvent>({}),
      ExecutionContextClient(&context),
      banner_service_remote_(&context),
      receiver_(this, &context),
      platforms_(platforms),
      user_choice_(MakeGarbageCollected<UserChoiceProperty>(&context)) {
  banner_service_remote_.Bind(
      std::move(service_remote),
      context.GetTaskRunner(TaskType::kApplicationLifeCycle));
  receiver_.Bind(std::move(event_receiver),
                 context.GetTaskRunner(TaskType::kApplicationLifeCycle));
  DCHECK(banner_service_remote_.is_bound());
  DCHECK(receiver_.is_bound());
  UseCounter::Count(context, WebFeature::kBeforeInstallPromptEvent);
}

BeforeInstallPromptEvent::BeforeInstallPromptEvent(
    ExecutionContext* execution_context,
    const AtomicString& name,
    const BeforeInstallPromptEventInit* init)
    : Event(name, init),
      ActiveScriptWrappable<BeforeInstallPromptEvent>({}),
      ExecutionContextClient(execution_context),
      banner_service_remote_(execution_context),
      receiver_(this, execution_context) {
  if (init->hasPlatforms())
    platforms_ = init->platforms();
}

BeforeInstallPromptEvent::~BeforeInstallPromptEvent() = default;

Vector<String> BeforeInstallPromptEvent::platforms() const {
  return platforms_;
}

ScriptPromise<AppBannerPromptResult> BeforeInstallPromptEvent::userChoice(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kBeforeInstallPromptEventUserChoice);
  // |m_binding| must be bound to allow the AppBannerService to resolve the
  // userChoice promise.
  if (user_choice_ && receiver_.is_bound())
    return user_choice_->Promise(script_state->World());
  exception_state.ThrowDOMException(
      DOMExceptionCode::kInvalidStateError,
      "userChoice cannot be accessed on this event.");
  return EmptyPromise();
}

ScriptPromise<AppBannerPromptResult> BeforeInstallPromptEvent::prompt(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // |m_bannerService| must be bound to allow us to inform the AppBannerService
  // to display the banner now.
  if (!banner_service_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The prompt() method cannot be called.");
    return EmptyPromise();
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!LocalFrame::ConsumeTransientUserActivation(window ? window->GetFrame()
                                                         : nullptr)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "The prompt() method must be called with a user gesture");
    return EmptyPromise();
  }

  UseCounter::Count(window, WebFeature::kBeforeInstallPromptEventPrompt);
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
         user_choice_->GetState() == UserChoiceProperty::kPending;
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

void BeforeInstallPromptEvent::Trace(Visitor* visitor) const {
  visitor->Trace(banner_service_remote_);
  visitor->Trace(receiver_);
  visitor->Trace(user_choice_);
  Event::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
