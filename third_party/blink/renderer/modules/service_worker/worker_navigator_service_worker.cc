// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/worker_navigator_service_worker.h"

#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_container.h"
#include "third_party/blink/renderer/modules/service_worker/worker_navigator_service_worker.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// static
ServiceWorkerContainer* WorkerNavigatorServiceWorker::serviceWorker(
    ScriptState* script_state,
    WorkerNavigator&,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context);

  if (!execution_context->GetSecurityOrigin()->CanAccessServiceWorkers()) {
    return nullptr;
  }

  auto* container = ServiceWorkerContainer::From(*execution_context);
  CHECK(container);

  return container;
}

}  // namespace blink
