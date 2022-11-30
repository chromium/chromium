// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_management.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/extensions_chromeos/v8/v8_cros_accelerator_event_init.h"
#include "third_party/blink/renderer/bindings/extensions_chromeos/v8/v8_cros_window_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/extensions/chromeos/event_target_chromeos_names.h"
#include "third_party/blink/renderer/extensions/chromeos/event_type_chromeos_names.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_accelerator_event.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_screen.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_event.h"

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

CrosWindowManagement::CrosWindowManagement(ExecutionContext& execution_context)
    : Supplement(execution_context),
      ExecutionContextClient(&execution_context) {
  // Set up a two way connection to the browser so we can make calls and receive
  // events.
  auto factory_receiver =
      cros_window_management_factory_.BindNewPipeAndPassReceiver(
          execution_context.GetTaskRunner(TaskType::kMiscPlatformAPI));
  execution_context.GetBrowserInterfaceBroker().GetInterface(
      std::move(factory_receiver));

  auto impl_receiver = cros_window_management_.BindNewEndpointAndPassReceiver(
      execution_context.GetTaskRunner(TaskType::kMiscPlatformAPI));
  auto observer_remote = observer_receiver_.BindNewEndpointAndPassRemote(
      execution_context.GetTaskRunner(TaskType::kMiscPlatformAPI));
  cros_window_management_factory_->Create(std::move(impl_receiver),
                                          std::move(observer_remote));
}

void CrosWindowManagement::Trace(Visitor* visitor) const {
  visitor->Trace(cros_window_management_factory_);
  visitor->Trace(cros_window_management_);
  visitor->Trace(observer_receiver_);
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
        WTF::BindOnce(&CrosWindowManagement::WindowsCallback,
                      WrapPersistent(this), WrapPersistent(resolver)));
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
        WTF::BindOnce(&CrosWindowManagement::ScreensCallback,
                      WrapPersistent(this), WrapPersistent(resolver)));
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
  DispatchEvent(*Event::Create(event_type_names::kStart));
}

void CrosWindowManagement::DispatchAcceleratorEvent(
    mojom::blink::AcceleratorEventPtr event) {
  auto* event_init = CrosAcceleratorEventInit::Create();
  event_init->setAcceleratorName(event->accelerator_name);
  event_init->setRepeat(event->repeat);
  const AtomicString& type =
      event->type == mojom::blink::AcceleratorEvent::Type::kDown
          ? event_type_names::kAcceleratordown
          : event_type_names::kAcceleratorup;
  DispatchEvent(*CrosAcceleratorEvent::Create(type, event_init));
}

void CrosWindowManagement::DispatchWindowOpenedEvent(
    mojom::blink::CrosWindowInfoPtr window) {
  WTF::String opened_id = WTF::String(window->id.ToString());

  // TODO(b/245442671): Currently event is dispatched asynchronously due to
  // waiting for CrosWindowManagementContext::GetCrosWindowManagement to
  // dispatch. This allows a race condition between populating the `windows_`
  // cache and dispatching open event. In the future, this shouldn't happen and
  // we should DCHECK() the newly opened window does not exist in cache.
  auto* cached_window_ptr =
      base::ranges::find(windows_, opened_id, &CrosWindow::id);

  auto* event_init = CrosWindowEventInit::Create();
  if (cached_window_ptr == windows_.end()) {
    auto* cros_window =
        MakeGarbageCollected<CrosWindow>(this, std::move(window));
    windows_.push_back(cros_window);
    event_init->setWindow(cros_window);
  } else {
    cached_window_ptr->Get()->Update(std::move(window));
    event_init->setWindow(cached_window_ptr->Get());
  }

  DispatchEvent(
      *CrosWindowEvent::Create(event_type_names::kWindowopened, event_init));
}

void CrosWindowManagement::DispatchWindowClosedEvent(
    mojom::blink::CrosWindowInfoPtr window) {
  WTF::String closed_window_id_string = WTF::String(window->id.ToString());
  Member<CrosWindow>* window_ptr =
      base::ranges::find(windows_, closed_window_id_string, &CrosWindow::id);

  auto* event_init = CrosWindowEventInit::Create();
  if (window_ptr == windows_.end()) {
    event_init->setWindow(
        MakeGarbageCollected<CrosWindow>(this, std::move(window)));
  } else {
    // Update cached CrosWindow member with CrosWindowInfoPtr with nulled
    // attributes.
    window_ptr->Get()->Update(std::move(window));
    event_init->setWindow(window_ptr->Get());
    windows_.erase(window_ptr);
  }

  DispatchEvent(
      *CrosWindowEvent::Create(event_type_names::kWindowclosed, event_init));
}

}  // namespace blink
