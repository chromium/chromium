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
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_screen.h"
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
      cros_window_management_factory_(&execution_context),
      cros_window_management_(&execution_context),
      receiver_(this, &execution_context) {
  auto factory_receiver =
      cros_window_management_factory_.BindNewPipeAndPassReceiver(
          execution_context.GetTaskRunner(TaskType::kMiscPlatformAPI));
  execution_context.GetBrowserInterfaceBroker().GetInterface(
      std::move(factory_receiver));

  auto impl_receiver = cros_window_management_.BindNewEndpointAndPassReceiver(
      execution_context.GetTaskRunner(TaskType::kMiscPlatformAPI));
  cros_window_management_factory_->Create(std::move(impl_receiver));
}

void CrosWindowManagement::Trace(Visitor* visitor) const {
  visitor->Trace(cros_window_management_factory_);
  visitor->Trace(cros_window_management_);
  visitor->Trace(receiver_);
  visitor->Trace(windows_);
  visitor->Trace(screens_);
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
    return nullptr;
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
    results.push_back(MakeGarbageCollected<CrosWindow>(this, std::move(w)));
  }

  windows_.swap(results);

  resolver->Resolve(windows_);
}

const HeapVector<Member<CrosWindow>>& CrosWindowManagement::windows() {
  return windows_;
}

ScriptPromise CrosWindowManagement::getScreens(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto* window_management = GetCrosWindowManagementOrNull();
  if (window_management) {
    window_management->GetAllScreens(
        WTF::Bind(&CrosWindowManagement::ScreensCallback, WrapPersistent(this),
                  WrapPersistent(resolver)));
  }
  return resolver->Promise();
}

void CrosWindowManagement::ScreensCallback(
    ScriptPromiseResolver* resolver,
    WTF::Vector<mojom::blink::CrosScreenInfoPtr> screens) {
  HeapVector<Member<CrosScreen>> results;
  results.ReserveInitialCapacity(screens.size());
  for (auto& s : screens) {
    results.push_back(MakeGarbageCollected<CrosScreen>(this, std::move(s)));
  }

  screens_.swap(results);

  resolver->Resolve(std::move(screens_));
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
