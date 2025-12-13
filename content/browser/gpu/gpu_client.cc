// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/gpu_client.h"

#include "content/browser/child_process_host_impl.h"
#include "content/browser/gpu/browser_gpu_client_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> CreateGpuClient(
    mojo::PendingReceiver<viz::mojom::Gpu> receiver,
    bool enable_extra_handles_validation) {
  // TODO(crbug.com/379869738): Refactor to use client_id without
  // GetUnsafeValue().
  const ChildProcessId client_id =
      ChildProcessHostImpl::GenerateChildProcessUniqueId();
  const uint64_t client_tracing_id =
      ChildProcessHostImpl::ChildProcessIdToTracingProcessId(client_id);
  auto task_runner = GetUIThreadTaskRunner({});
  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client(
      new viz::GpuClient(std::make_unique<BrowserGpuClientDelegate>(),
                         client_id.GetUnsafeValue(), client_tracing_id,
                         enable_extra_handles_validation, task_runner),
      base::OnTaskRunnerDeleter(task_runner));
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&viz::GpuClient::Add, gpu_client->GetWeakPtr(),
                                std::move(receiver)));
  return gpu_client;
}

}  // namespace content
