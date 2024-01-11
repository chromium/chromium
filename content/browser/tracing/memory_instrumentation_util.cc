// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/memory_instrumentation_util.h"

#include "base/trace_event/trace_event.h"
#include "content/public/browser/resource_coordinator_service.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace content {

void InitializeBrowserMemoryInstrumentationClient() {
  auto task_runner = base::trace_event::MemoryDumpManager::GetInstance()
                         ->GetDumpThreadTaskRunner();
  if (!task_runner->RunsTasksInCurrentSequence()) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&InitializeBrowserMemoryInstrumentationClient));
    return;
  }
  TRACE_EVENT0("startup", "InitializeBrowserMemoryInstrumentationClient");
  mojo::PendingRemote<memory_instrumentation::mojom::Coordinator> coordinator;
  mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess> process;
  auto process_receiver = process.InitWithNewPipeAndPassReceiver();
  GetMemoryInstrumentationRegistry()->RegisterClientProcess(
      coordinator.InitWithNewPipeAndPassReceiver(), std::move(process),
      memory_instrumentation::mojom::ProcessType::BROWSER,
      base::GetCurrentProcId(), /*service_name=*/std::nullopt);
  memory_instrumentation::ClientProcessImpl::CreateInstance(
      std::move(process_receiver), std::move(coordinator),
      /*is_browser_process=*/true);
}

}  // namespace content
