// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/gpu_client.h"

#include "content/browser/gpu/browser_gpu_client_delegate.h"
#include "content/common/child_process_host_impl.h"

namespace content {

std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> CreateGpuClient(
    mojo::PendingReceiver<viz::mojom::Gpu> receiver,
    viz::GpuClient::ConnectionErrorHandlerClosure connection_error_handler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  const int client_id = ChildProcessHostImpl::GenerateChildProcessUniqueId();
  const uint64_t client_tracing_id =
      ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(client_id);
  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client(
      new viz::GpuClient(std::make_unique<BrowserGpuClientDelegate>(),
                         client_id, client_tracing_id, task_runner),
      base::OnTaskRunnerDeleter(task_runner));
  gpu_client->SetConnectionErrorHandler(std::move(connection_error_handler));
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&viz::GpuClient::Add, gpu_client->GetWeakPtr(),
                                std::move(receiver)));
  return gpu_client;
}

}  // namespace content
