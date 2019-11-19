// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/modules/badging/navigator_badge.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

const char NavigatorBadge::kSupplementName[] = "NavigatorBadge";

// static
NavigatorBadge& NavigatorBadge::From(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  NavigatorBadge* supplement =
      Supplement<ExecutionContext>::From<NavigatorBadge>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorBadge>(context);
    ProvideTo(*context, supplement);
  }
  return *supplement;
}

NavigatorBadge::NavigatorBadge(ExecutionContext* context) {
  context->GetBrowserInterfaceBroker().GetInterface(
      badge_service_.BindNewPipeAndPassReceiver());
  DCHECK(badge_service_);
}

// static
ScriptPromise NavigatorBadge::setAppBadge(ScriptState* script_state,
                                          Navigator& /*navigator*/) {
  From(script_state)
      .badge_service_->SetBadge(mojom::blink::BadgeValue::NewFlag(0));
  return ScriptPromise::CastUndefined(script_state);
}

// static
ScriptPromise NavigatorBadge::setAppBadge(ScriptState* script_state,
                                          Navigator& navigator,
                                          uint64_t content) {
  if (content == 0)
    return NavigatorBadge::clearAppBadge(script_state, navigator);

  From(script_state)
      .badge_service_->SetBadge(mojom::blink::BadgeValue::NewNumber(content));
  return ScriptPromise::CastUndefined(script_state);
}

// static
ScriptPromise NavigatorBadge::clearAppBadge(ScriptState* script_state,
                                            Navigator& /*navigator*/) {
  From(script_state).badge_service_->ClearBadge();
  return ScriptPromise::CastUndefined(script_state);
}

void NavigatorBadge::Trace(blink::Visitor* visitor) {
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
