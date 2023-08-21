// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/diagnostics/cros_diagnostics.h"

#include "third_party/blink/public/mojom/chromeos/diagnostics/cros_diagnostics.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/extensions_chromeos/v8/v8_cros_cpu_info.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

const char CrosDiagnostics::kSupplementName[] = "CrosDiagnostics";

CrosDiagnostics& CrosDiagnostics::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());
  auto* supplement =
      Supplement<ExecutionContext>::From<CrosDiagnostics>(execution_context);
  if (!supplement) {
    supplement = MakeGarbageCollected<CrosDiagnostics>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

CrosDiagnostics::CrosDiagnostics(ExecutionContext& execution_context)
    : Supplement(execution_context),
      ExecutionContextClient(&execution_context),
      cros_diagnostics_(&execution_context) {}

mojom::blink::CrosDiagnostics* CrosDiagnostics::GetCrosDiagnosticsOrNull() {
  auto* execution_context = GetExecutionContext();
  if (!execution_context) {
    return nullptr;
  }

  if (!cros_diagnostics_.is_bound()) {
    auto receiver = cros_diagnostics_.BindNewPipeAndPassReceiver(
        execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        std::move(receiver));
  }
  return cros_diagnostics_.get();
}

void CrosDiagnostics::Trace(Visitor* visitor) const {
  visitor->Trace(cros_diagnostics_);
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise CrosDiagnostics::getCpuInfo(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto* cros_diagnostics = GetCrosDiagnosticsOrNull();

  if (cros_diagnostics) {
    cros_diagnostics->GetCpuInfo(
        WTF::BindOnce(&CrosDiagnostics::OnGetCpuInfoResponse,
                      WrapPersistent(this), WrapPersistent(resolver)));
  }

  return resolver->Promise();
}

void CrosDiagnostics::OnGetCpuInfoResponse(
    ScriptPromiseResolver* resolver,
    mojom::blink::CrosCpuInfoPtr cpu_info_mojom) {
  auto* cpu_info_blink = MakeGarbageCollected<CrosCpuInfo>();

  cpu_info_blink->setArchitectureName(cpu_info_mojom->architecture_name);
  cpu_info_blink->setModelName(cpu_info_mojom->model_name);
  cpu_info_blink->setNumOfProcessors(cpu_info_mojom->num_of_processors);

  resolver->Resolve(cpu_info_blink);
}

}  // namespace blink
