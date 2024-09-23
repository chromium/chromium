// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/badging/navigator_badge.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/modules/notifications/notification_manager.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

const char NavigatorBadge::kSupplementName[] = "NavigatorBadge";

// static
NavigatorBadge& NavigatorBadge::From(ScriptState* script_state) {
  DCHECK(IsAllowed(script_state));
  ExecutionContext* context = ExecutionContext::From(script_state);
  NavigatorBadge* supplement =
      Supplement<ExecutionContext>::From<NavigatorBadge>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorBadge>(context);
    ProvideTo(*context, supplement);
  }
  return *supplement;
}

NavigatorBadge::NavigatorBadge(ExecutionContext* context)
    : Supplement(*context) {}

// static
ScriptPromise<IDLUndefined> NavigatorBadge::setAppBadge(
    ScriptState* script_state,
    Navigator& /*navigator*/,
    ExceptionState& exception_state) {
  return SetAppBadgeHelper(script_state, mojom::blink::BadgeValue::NewFlag(0),
                           exception_state);
}

// static
ScriptPromise<IDLUndefined> NavigatorBadge::setAppBadge(
    ScriptState* script_state,
    WorkerNavigator& /*navigator*/,
    ExceptionState& exception_state) {
  return SetAppBadgeHelper(script_state, mojom::blink::BadgeValue::NewFlag(0),
                           exception_state);
}

// static
ScriptPromise<IDLUndefined> NavigatorBadge::setAppBadge(
    ScriptState* script_state,
    Navigator& /*navigator*/,
    uint64_t content,
    ExceptionState& exception_state) {
  return SetAppBadgeHelper(script_state,
                           mojom::blink::BadgeValue::NewNumber(content),
                           exception_state);
}

// static
ScriptPromise<IDLUndefined> NavigatorBadge::setAppBadge(
    ScriptState* script_state,
    WorkerNavigator& /*navigator*/,
    uint64_t content,
    ExceptionState& exception_state) {
  return SetAppBadgeHelper(script_state,
                           mojom::blink::BadgeValue::NewNumber(content),
                           exception_state);
}

// static
ScriptPromise<IDLUndefined> NavigatorBadge::clearAppBadge(
    ScriptState* script_state,
    Navigator& /*navigator*/,
    ExceptionState& exception_state) {
  return ClearAppBadgeHelper(script_state, exception_state);
}

// static
ScriptPromise<IDLUndefined> NavigatorBadge::clearAppBadge(
    ScriptState* script_state,
    WorkerNavigator& /*navigator*/,
    ExceptionState& exception_state) {
  return ClearAppBadgeHelper(script_state, exception_state);
}

void NavigatorBadge::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
}

// static
ScriptPromise<IDLUndefined> NavigatorBadge::SetAppBadgeHelper(
    ScriptState* script_state,
    mojom::blink::BadgeValuePtr badge_value,
    ExceptionState& exception_state) {
  if (badge_value->is_number() && badge_value->get_number() == 0)
    return ClearAppBadgeHelper(script_state, exception_state);

  if (!IsAllowed(script_state)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "The badge API is not allowed in this context");
    return EmptyPromise();
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/1413916): The service is implemented in Chrome, so it may
  // not be provided in other embedders. Ensure that case is handled properly.
  From(script_state).badge_service()->SetBadge(std::move(badge_value));
#endif

  ExecutionContext* context = ExecutionContext::From(script_state);
  if (context) {
    mojom::blink::WebFeature feature =
        context->IsWindow()
            ? mojom::blink::WebFeature::
                  kBadgeSetWithoutNotificationPermissionInBrowserWindow
            : mojom::blink::WebFeature::
                  kBadgeSetWithoutNotificationPermissionInWorker;
    if (context->IsWindow()) {
      LocalFrame* frame = DynamicTo<LocalDOMWindow>(context)->GetFrame();
      if (frame && frame->GetSettings() &&
          !frame->GetSettings()->GetWebAppScope().empty()) {
        feature = mojom::blink::WebFeature::
            kBadgeSetWithoutNotificationPermissionInAppWindow;
      }
    }
    NotificationManager::From(context)->GetPermissionStatusAsync(WTF::BindOnce(
        [](mojom::blink::WebFeature feature, UseCounter* counter,
           mojom::blink::PermissionStatus status) {
          if (status != mojom::blink::PermissionStatus::GRANTED) {
            UseCounter::Count(counter, feature);
          }
        },
        feature, WrapWeakPersistent(context)));
  }
  return ToResolvedUndefinedPromise(script_state);
}

// static
ScriptPromise<IDLUndefined> NavigatorBadge::ClearAppBadgeHelper(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!IsAllowed(script_state)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "The badge API is not allowed in this context");
    return EmptyPromise();
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/1413916): The service is implemented in Chrome, so it may
  // not be provided in other embedders. Ensure that case is handled properly.
  From(script_state).badge_service()->ClearBadge();
#endif
  return ToResolvedUndefinedPromise(script_state);
}

// static
bool NavigatorBadge::IsAllowed(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  return !context->IsInFencedFrame();
}

mojo::Remote<mojom::blink::BadgeService> NavigatorBadge::badge_service() {
  mojo::Remote<mojom::blink::BadgeService> badge_service;
  GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
      badge_service.BindNewPipeAndPassReceiver());
  DCHECK(badge_service);

  return badge_service;
}

}  // namespace blink
