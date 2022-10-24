// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/managed_device_health_services/telemetry/cros_telemetry.h"

#include <utility>

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"

namespace blink {

const char CrosTelemetry::kSupplementName[] = "Telemetry";

CrosTelemetry& CrosTelemetry::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());
  auto* supplement =
      Supplement<ExecutionContext>::From<CrosTelemetry>(execution_context);
  if (!supplement) {
    supplement = MakeGarbageCollected<CrosTelemetry>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

CrosTelemetry::CrosTelemetry(ExecutionContext& execution_context)
    : Supplement(execution_context),
      ExecutionContextClient(&execution_context),
      remote_telemetry_service_(&execution_context) {}

void CrosTelemetry::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
  visitor->Trace(remote_telemetry_service_);
}

mojom::blink::CrosTelemetry* CrosTelemetry::GetCrosTelemetryServiceOrNull() {
  auto* execution_context = GetExecutionContext();
  if (!execution_context) {
    return nullptr;
  }

  if (!remote_telemetry_service_.is_bound()) {
    auto receiver = remote_telemetry_service_.BindNewPipeAndPassReceiver(
        execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        std::move(receiver));
  }

  return remote_telemetry_service_.get();
}

}  // namespace blink
