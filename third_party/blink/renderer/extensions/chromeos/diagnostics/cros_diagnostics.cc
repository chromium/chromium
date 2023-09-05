// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/diagnostics/cros_diagnostics.h"

#include "third_party/blink/public/mojom/chromeos/diagnostics/cros_diagnostics.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/extensions_chromeos/v8/v8_cros_cpu_info.h"
#include "third_party/blink/renderer/bindings/extensions_chromeos/v8/v8_cros_logical_cpu_info.h"
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
      cros_diagnostics_remote_(&execution_context) {}

mojom::blink::CrosDiagnostics* CrosDiagnostics::GetCrosDiagnosticsOrNull() {
  auto* execution_context = GetExecutionContext();
  if (!execution_context) {
    return nullptr;
  }

  if (!cros_diagnostics_remote_.is_bound()) {
    auto receiver = cros_diagnostics_remote_.BindNewPipeAndPassReceiver(
        execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        std::move(receiver));
  }
  return cros_diagnostics_remote_.get();
}

void CrosDiagnostics::Trace(Visitor* visitor) const {
  visitor->Trace(cros_diagnostics_remote_);
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
    mojom::blink::GetCpuInfoResultPtr result) {
  if (result->is_error()) {
    switch (result->get_error()) {
      case mojom::blink::GetCpuInfoError::kTelemetryProbeServiceUnavailable:
        resolver->Reject("TelemetryProbeService is unavailable.");
        return;
      case mojom::blink::GetCpuInfoError::kCpuTelemetryInfoUnavailable:
        resolver->Reject(
            "TelemetryProbeService returned an error when retrieving CPU "
            "telemetry info.");
        return;
    }
    NOTREACHED_NORETURN();
  }

  CHECK(result->is_cpu_info());
  auto* cpu_info_blink = MakeGarbageCollected<CrosCpuInfo>();

  cpu_info_blink->setArchitectureName(
      result->get_cpu_info()->architecture_name);
  cpu_info_blink->setModelName(result->get_cpu_info()->model_name);
  cpu_info_blink->setNumOfEfficientProcessors(
      result->get_cpu_info()->num_of_efficient_processors);

  HeapVector<Member<CrosLogicalCpuInfo>> logical_cpu_infos_blink;
  for (const auto& logical_cpu : result->get_cpu_info()->logical_cpus) {
    auto* logical_cpu_info_blink = MakeGarbageCollected<CrosLogicalCpuInfo>();

    logical_cpu_info_blink->setCoreId(logical_cpu->core_id);
    // While `logical_cpu->idle_time_ms` is of type uint64_t, the maximum safe
    // integer returnable to JavaScript is 2^53 - 1, which is roughly equivalent
    // to 285616 years of idle time. For any practical purposes, it is safe to
    // return `logical_cpu->idle_time_ms` as-is.
    logical_cpu_info_blink->setIdleTimeMs(logical_cpu->idle_time_ms);
    logical_cpu_info_blink->setMaxClockSpeedKhz(
        logical_cpu->max_clock_speed_khz);
    logical_cpu_info_blink->setScalingCurrentFrequencyKhz(
        logical_cpu->scaling_current_frequency_khz);
    logical_cpu_info_blink->setScalingMaxFrequencyKhz(
        logical_cpu->scaling_max_frequency_khz);

    logical_cpu_infos_blink.push_back(std::move(logical_cpu_info_blink));
  }

  cpu_info_blink->setLogicalCpus(logical_cpu_infos_blink);
  resolver->Resolve(std::move(cpu_info_blink));
}

}  // namespace blink
