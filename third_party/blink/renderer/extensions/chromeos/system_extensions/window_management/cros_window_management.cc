// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_management.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window.h"

namespace blink {

CrosWindowManagement::CrosWindowManagement(ExecutionContext* execution_context)
    : ExecutionContextClient(execution_context),
      cros_window_management_(execution_context) {}

void CrosWindowManagement::Trace(Visitor* visitor) const {
  visitor->Trace(cros_window_management_);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
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

ScriptPromise CrosWindowManagement::windows(ScriptState* script_state) {
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
    WTF::Vector<mojom::blink::CrosWindowPtr> windows) {
  HeapVector<Member<CrosWindow>> results;
  results.ReserveInitialCapacity(windows.size());
  for (auto& w : windows) {
    auto* result = MakeGarbageCollected<CrosWindow>(this, std::move(w));
    results.push_back(result);
  }
  resolver->Resolve(results);
}

}  // namespace blink
