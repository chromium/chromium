// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_request.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_callbacks.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection_callbacks.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

bool IsKnownProtocolForPresentationUrl(const KURL& url) {
  return url.ProtocolIsInHTTPFamily() || url.ProtocolIs("cast") ||
         url.ProtocolIs("cast-dial");
}

}  // anonymous namespace

// static
PresentationRequest* PresentationRequest::Create(
    ExecutionContext* execution_context,
    const String& url,
    ExceptionState& exception_state) {
  Vector<String> urls(1);
  urls[0] = url;
  return Create(execution_context, urls, exception_state);
}

PresentationRequest* PresentationRequest::Create(
    ExecutionContext* execution_context,
    const Vector<String>& urls,
    ExceptionState& exception_state) {
  if (execution_context->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kPresentationController)) {
    exception_state.ThrowSecurityError(
        DynamicTo<LocalDOMWindow>(execution_context)
                ->GetFrame()
                ->IsInFencedFrameTree()
            ? "PresentationRequest is not supported in a fenced frame tree."
            : "The document is sandboxed and lacks the 'allow-presentation' "
              "flag.");
    return nullptr;
  }

  Vector<KURL> parsed_urls;
  for (const auto& url : urls) {
    const KURL& parsed_url = KURL(execution_context->Url(), url);

    if (!parsed_url.IsValid()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "'" + url + "' can't be resolved to a valid URL.");
      return nullptr;
    }

    if (parsed_url.ProtocolIsInHTTPFamily() &&
        MixedContentChecker::IsMixedContent(
            execution_context->GetSecurityOrigin(), parsed_url)) {
      exception_state.ThrowSecurityError(
          "Presentation of an insecure document [" + url +
          "] is prohibited from a secure context.");
      return nullptr;
    }

    if (IsKnownProtocolForPresentationUrl(parsed_url))
      parsed_urls.push_back(parsed_url);
  }

  if (parsed_urls.IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Do not support empty sequence of URLs.");
    return nullptr;
  }

  return MakeGarbageCollected<PresentationRequest>(execution_context,
                                                   parsed_urls);
}

const AtomicString& PresentationRequest::InterfaceName() const {
  return event_target_names::kPresentationRequest;
}

ExecutionContext* PresentationRequest::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void PresentationRequest::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == event_type_names::kConnectionavailable) {
    UseCounter::Count(
        GetExecutionContext(),
        WebFeature::kPresentationRequestConnectionAvailableEventListener);
  }
}

bool PresentationRequest::HasPendingActivity() const {
  // Prevents garbage collecting of this object when not hold by another
  // object but still has listeners registered.
  if (!GetExecutionContext())
    return false;

  if (HasEventListeners())
    return true;

  return availability_property_ &&
         availability_property_->GetState() ==
             PresentationAvailabilityProperty::kPending;
}

ScriptPromise PresentationRequest::start(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The PresentationRequest is no longer associated to a frame.");
    return ScriptPromise();
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (window->GetFrame()->GetSettings()->GetPresentationRequiresUserGesture() &&
      !LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "PresentationRequest::start() requires user gesture.");
    return ScriptPromise();
  }

  PresentationController* controller = PresentationController::From(*window);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  controller->GetPresentationService()->StartPresentation(
      urls_,
      WTF::Bind(
          &PresentationConnectionCallbacks::HandlePresentationResponse,
          std::make_unique<PresentationConnectionCallbacks>(resolver, this)));
  return resolver->Promise();
}

ScriptPromise PresentationRequest::reconnect(ScriptState* script_state,
                                             const String& id,
                                             ExceptionState& exception_state) {
  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The PresentationRequest is no longer associated to a frame.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  ControllerPresentationConnection* existing_connection =
      controller->FindExistingConnection(urls_, id);
  if (existing_connection) {
    controller->GetPresentationService()->ReconnectPresentation(
        urls_, id,
        WTF::Bind(&PresentationConnectionCallbacks::HandlePresentationResponse,
                  std::make_unique<PresentationConnectionCallbacks>(
                      resolver, existing_connection)));
  } else {
    controller->GetPresentationService()->ReconnectPresentation(
        urls_, id,
        WTF::Bind(
            &PresentationConnectionCallbacks::HandlePresentationResponse,
            std::make_unique<PresentationConnectionCallbacks>(resolver, this)));
  }
  return resolver->Promise();
}

ScriptPromise PresentationRequest::getAvailability(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The PresentationRequest is no longer associated to a frame.");
    return ScriptPromise();
  }

  if (!availability_property_) {
    availability_property_ =
        MakeGarbageCollected<PresentationAvailabilityProperty>(
            ExecutionContext::From(script_state));

    controller->GetAvailabilityState()->RequestAvailability(
        urls_, MakeGarbageCollected<PresentationAvailabilityCallbacks>(
                   availability_property_, urls_));
  }
  return availability_property_->Promise(script_state->World());
}

const Vector<KURL>& PresentationRequest::Urls() const {
  return urls_;
}

void PresentationRequest::Trace(Visitor* visitor) const {
  visitor->Trace(availability_property_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

PresentationRequest::PresentationRequest(ExecutionContext* execution_context,
                                         const Vector<KURL>& urls)
    : ExecutionContextClient(execution_context), urls_(urls) {}

}  // namespace blink
