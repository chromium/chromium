// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_request.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_latency.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_presentation_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_presentationsource_usvstring.h"
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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

bool IsKnownProtocolForPresentationUrl(const KURL& url) {
  return url.ProtocolIsInHTTPFamily() || url.ProtocolIs("cast") ||
         url.ProtocolIs("cast-dial");
}

int GetPlayoutDelay(const PresentationSource& source) {
  if (!source.hasLatencyHint() || !source.latencyHint()) {
    return 400;
  }
  switch (source.latencyHint()->AsEnum()) {
    case V8CaptureLatency::Enum::kLow:
      return 200;
    case V8CaptureLatency::Enum::kDefault:
      return 400;
    case V8CaptureLatency::Enum::kHigh:
      return 800;
  }
}

KURL CreateMirroringUrl(const PresentationSource& source) {
  int capture_audio = !source.hasAudioPlayback() || !source.audioPlayback() ||
                              (source.audioPlayback()->AsEnum() ==
                               V8AudioPlaybackDestination::Enum::kReceiver)
                          ? 1
                          : 0;
  int playout_delay = GetPlayoutDelay(source);
  // TODO(crbug.com/1267372): Instead of converting a mirroring source into a
  // URL with a hardcoded Cast receiver app ID, pass the source object directly
  // to the embedder.
  return KURL(
      String::Format("cast:0F5096E8?streamingCaptureAudio=%d&"
                     "streamingTargetPlayoutDelayMillis=%d",
                     capture_audio, playout_delay));
}

KURL CreateUrlFromSource(const ExecutionContext& execution_context,
                         const PresentationSource& source) {
  if (!source.hasType()) {
    return KURL();
  }
  switch (source.type().AsEnum()) {
    case V8PresentationSourceType::Enum::kUrl:
      return source.hasUrl() ? KURL(execution_context.Url(), source.url())
                             : KURL();
    case V8PresentationSourceType::Enum::kMirroring:
      return CreateMirroringUrl(source);
  }
}

}  // anonymous namespace

// static
PresentationRequest* PresentationRequest::Create(
    ExecutionContext* execution_context,
    const String& url,
    ExceptionState& exception_state) {
  HeapVector<Member<V8UnionPresentationSourceOrUSVString>> urls(1);
  urls[0] = MakeGarbageCollected<V8UnionPresentationSourceOrUSVString>(url);
  return Create(execution_context, urls, exception_state);
}

// static
PresentationRequest* PresentationRequest::Create(
    ExecutionContext* execution_context,
    const HeapVector<Member<V8UnionPresentationSourceOrUSVString>>& sources,
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
  for (const auto& source : sources) {
    if (source->IsPresentationSource()) {
      if (!RuntimeEnabledFeatures::SiteInitiatedMirroringEnabled()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotSupportedError,
            "You must pass in valid URL strings.");
        return nullptr;
      }
      const KURL source_url = CreateUrlFromSource(
          *execution_context, *source->GetAsPresentationSource());
      if (!source_url.IsValid()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotSupportedError,
            "You must pass in valid presentation sources.");
        return nullptr;
      }
      parsed_urls.push_back(source_url);
      continue;
    }
    DCHECK(source->IsUSVString());
    const String& url = source->GetAsUSVString();
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

  if (parsed_urls.empty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "An empty sequence of URLs is not supported.");
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
  EventTarget::AddedEventListener(event_type, registered_listener);
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

ScriptPromise<PresentationConnection> PresentationRequest::start(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The PresentationRequest is no longer associated to a frame.");
    return EmptyPromise();
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (window->GetFrame()->GetSettings()->GetPresentationRequiresUserGesture() &&
      !LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "PresentationRequest::start() requires user gesture.");
    return EmptyPromise();
  }

  PresentationController* controller = PresentationController::From(*window);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationConnection>>(
          script_state, exception_state.GetContext());

  controller->GetPresentationService()->StartPresentation(
      urls_,
      WTF::BindOnce(
          &PresentationConnectionCallbacks::HandlePresentationResponse,
          std::make_unique<PresentationConnectionCallbacks>(resolver, this)));
  return resolver->Promise();
}

ScriptPromise<PresentationConnection> PresentationRequest::reconnect(
    ScriptState* script_state,
    const String& id,
    ExceptionState& exception_state) {
  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The PresentationRequest is no longer associated to a frame.");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationConnection>>(
          script_state, exception_state.GetContext());

  ControllerPresentationConnection* existing_connection =
      controller->FindExistingConnection(urls_, id);
  if (existing_connection) {
    controller->GetPresentationService()->ReconnectPresentation(
        urls_, id,
        WTF::BindOnce(
            &PresentationConnectionCallbacks::HandlePresentationResponse,
            std::make_unique<PresentationConnectionCallbacks>(
                resolver, existing_connection)));
  } else {
    controller->GetPresentationService()->ReconnectPresentation(
        urls_, id,
        WTF::BindOnce(
            &PresentationConnectionCallbacks::HandlePresentationResponse,
            std::make_unique<PresentationConnectionCallbacks>(resolver, this)));
  }
  return resolver->Promise();
}

ScriptPromise<PresentationAvailability> PresentationRequest::getAvailability(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The PresentationRequest is no longer associated to a frame.");
    return EmptyPromise();
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
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

PresentationRequest::PresentationRequest(ExecutionContext* execution_context,
                                         const Vector<KURL>& urls)
    : ActiveScriptWrappable<PresentationRequest>({}),
      ExecutionContextClient(execution_context),
      urls_(urls) {}

}  // namespace blink
