// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/memory_instrumentation_util.h"

#include "content/public/browser/resource_coordinator_service.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace content {

void InitializeBrowserMemoryInstrumentationClient() {
  mojo::PendingRemote<memory_instrumentation::mojom::Coordinator> coordinator;
  mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess> process;
  auto process_receiver = process.InitWithNewPipeAndPassReceiver();
  GetMemoryInstrumentationCoordinatorController()->RegisterClientProcess(
      coordinator.InitWithNewPipeAndPassReceiver(), std::move(process),
      memory_instrumentation::mojom::ProcessType::BROWSER,
      base::GetCurrentProcId(), /*service_name=*/base::nullopt);
  memory_instrumentation::ClientProcessImpl::CreateInstance(
      std::move(process_receiver), std::move(coordinator),
      /*is_browser_process=*/true);
}

}  // namespace content
