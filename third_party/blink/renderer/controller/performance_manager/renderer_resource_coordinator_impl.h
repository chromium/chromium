// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_PERFORMANCE_MANAGER_RENDERER_RESOURCE_COORDINATOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_PERFORMANCE_MANAGER_RENDERER_RESOURCE_COORDINATOR_IMPL_H_

#include <optional>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-blink.h"
#include "components/performance_manager/public/mojom/v8_contexts.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CONTROLLER_EXPORT RendererResourceCoordinatorImpl final
    : public RendererResourceCoordinator {
  USING_FAST_MALLOC(RendererResourceCoordinatorImpl);

 public:
  RendererResourceCoordinatorImpl(const RendererResourceCoordinatorImpl&) =
      delete;
  RendererResourceCoordinatorImpl& operator=(
      const RendererResourceCoordinatorImpl&) = delete;
  ~RendererResourceCoordinatorImpl() final;

  // Only initializes if the instrumentation runtime feature is enabled.
  static void MaybeInitialize();

  // RendererResourceCoordinator:
  void SetMainThreadTaskLoadIsLow(bool) final;
  void OnScriptStateCreated(ScriptState* script_state,
                            ExecutionContext* execution_context) final;
  void OnScriptStateDetached(ScriptState* script_state) final;
  void OnScriptStateDestroyed(ScriptState* script_state) final;
  void OnBeforeContentFrameAttached(const Frame& frame,
                                    const HTMLFrameOwnerElement& owner) final;
  void OnBeforeContentFrameDetached(const Frame& frame,
                                    const HTMLFrameOwnerElement& owner) final;

 private:
  friend class RendererResourceCoordinatorImplTest;

  explicit RendererResourceCoordinatorImpl(
      mojo::PendingRemote<
          performance_manager::mojom::blink::ProcessCoordinationUnit> remote);

  // Used for dispatching script state events which can arrive on any thread
  // but need to be sent outbound from the main thread.
  void DispatchOnV8ContextCreated(
      performance_manager::mojom::blink::V8ContextDescriptionPtr v8_desc,
      performance_manager::mojom::blink::IframeAttributionDataPtr
          iframe_attribution_data);
  void DispatchOnV8ContextDetached(const blink::V8ContextToken& token);
  void DispatchOnV8ContextDestroyed(const blink::V8ContextToken& token);
  void DispatchFireBackgroundTracingTrigger(const String& trigger_name);

  // Receives the results of
  // ProcessCoordinationUnit::RequestSharedPerformanceScenarioRegions().
  void OnSharedPerformanceScenarioRegions(
      base::ReadOnlySharedMemoryRegion global_region,
      base::ReadOnlySharedMemoryRegion process_region);

  mojo::Remote<performance_manager::mojom::blink::ProcessCoordinationUnit>
      service_;
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;

  // Scopers to manage the lifetime of shared memory regions for performance
  // scenarios. These regions are read by functions in
  // //third_party/blink/public/common/performance/performance_scenarios.h.
  std::optional<blink::performance_scenarios::ScopedReadOnlyScenarioMemory>
      process_performance_scenario_memory_;
  std::optional<blink::performance_scenarios::ScopedReadOnlyScenarioMemory>
      global_performance_scenario_memory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_PERFORMANCE_MANAGER_RENDERER_RESOURCE_COORDINATOR_IMPL_H_
