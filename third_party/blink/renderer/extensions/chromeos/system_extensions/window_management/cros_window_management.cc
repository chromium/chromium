// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_management.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/extensions/chromeos/event_target_chromeos.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window.h"

namespace blink {

const char CrosWindowManagement::kSupplementName[] = "CrosWindowManagement";

CrosWindowManagement& CrosWindowManagement::From(
    ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());
  auto* supplement = Supplement<ExecutionContext>::From<CrosWindowManagement>(
      execution_context);
  if (!supplement) {
    supplement = MakeGarbageCollected<CrosWindowManagement>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

void CrosWindowManagement::BindWindowManagerStartObserver(
    ExecutionContext* execution_context,
    mojo::PendingReceiver<mojom::blink::CrosWindowManagementStartObserver>
        receiver) {
  // The InterfaceProvider pipe, which calls binders, is destroyed before the
  // ExecutionContext, so `execution_context` should never be null.
  DCHECK(execution_context);
  auto& window_management = CrosWindowManagement::From(*execution_context);
  window_management.BindWindowManagerStartObserverImpl(std::move(receiver));
}

CrosWindowManagement::CrosWindowManagement(ExecutionContext& execution_context)
    : Supplement(execution_context),
      ExecutionContextClient(&execution_context),
      cros_window_management_(&execution_context),
      receiver_(this, &execution_context) {}

void CrosWindowManagement::Trace(Visitor* visitor) const {
  visitor->Trace(cros_window_management_);
  visitor->Trace(receiver_);
  Supplement<ExecutionContext>::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

const WTF::AtomicString& CrosWindowManagement::InterfaceName() const {
  return event_target_names::kCrosWindowManagement;
}

ExecutionContext* CrosWindowManagement::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

mojom::blink::CrosWindowManagement*
CrosWindowManagement::GetCrosWindowManagementOrNull() {
  auto* execution_context = GetExecutionContext();
  if (!execution_context) {
    return nullptr;
  }

  if (!cros_window_management_.is_bound()) {
    auto receiver = cros_window_management_.BindNewPipeAndPassReceiver(
        execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        std::move(receiver));
  }
  return cros_window_management_.get();
}

ScriptPromise CrosWindowManagement::getWindows(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto* window_management = GetCrosWindowManagementOrNull();
  if (window_management) {
    window_management->GetAllWindows(
        WTF::Bind(&CrosWindowManagement::WindowsCallback, WrapPersistent(this),
                  WrapPersistent(resolver)));
  }
  return resolver->Promise();
}

void CrosWindowManagement::WindowsCallback(
    ScriptPromiseResolver* resolver,
    WTF::Vector<mojom::blink::CrosWindowInfoPtr> windows) {
  HeapVector<Member<CrosWindow>> results;
  results.ReserveInitialCapacity(windows.size());
  for (auto& w : windows) {
    auto* result = MakeGarbageCollected<CrosWindow>(this, std::move(w));
    results.push_back(result);
  }
  resolver->Resolve(results);
}

void CrosWindowManagement::DispatchStartEvent() {
  DLOG(INFO) << "Dispatching event";
  DispatchEvent(*Event::Create(event_type_names::kStart));
}

void CrosWindowManagement::BindWindowManagerStartObserverImpl(
    mojo::PendingReceiver<mojom::blink::CrosWindowManagementStartObserver>
        receiver) {
  receiver_.Bind(std::move(receiver), GetSupplementable()->GetTaskRunner(
                                          TaskType::kInternalDefault));
}

}  // namespace blink
