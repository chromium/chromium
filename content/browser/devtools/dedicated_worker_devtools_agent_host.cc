// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/dedicated_worker_devtools_agent_host.h"

#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/target_auto_attacher.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"

namespace content {

// static
DedicatedWorkerDevToolsAgentHost* DedicatedWorkerDevToolsAgentHost::GetFor(
    const DedicatedWorkerHost* host) {
  return WorkerDevToolsManager::GetInstance().GetDevToolsHost(host);
}

DedicatedWorkerDevToolsAgentHost::DedicatedWorkerDevToolsAgentHost(
    int process_id,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& devtools_worker_token,
    const std::string& parent_id,
    base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback)
    : WorkerOrWorkletDevToolsAgentHost(process_id,
                                       url,
                                       name,
                                       devtools_worker_token,
                                       parent_id,
                                       std::move(destroyed_callback)),
      auto_attacher_(std::make_unique<protocol::RendererAutoAttacherBase>(
          GetRendererChannel())) {
  NotifyCreated();
}

DedicatedWorkerDevToolsAgentHost::~DedicatedWorkerDevToolsAgentHost() = default;

std::string DedicatedWorkerDevToolsAgentHost::GetType() {
  return kTypeDedicatedWorker;
}

DedicatedWorkerHost*
DedicatedWorkerDevToolsAgentHost::GetDedicatedWorkerHost() {
  RenderProcessHost* const process = GetProcessHost();
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(process->GetStoragePartition());
  auto* service = storage_partition_impl->GetDedicatedWorkerService();
  return service->GetDedicatedWorkerHostFromToken(
      blink::DedicatedWorkerToken(devtools_worker_token()));
}

bool DedicatedWorkerDevToolsAgentHost::AttachSession(DevToolsSession* session,
                                                     bool acquire_wake_lock) {
  session->CreateAndAddHandler<protocol::IOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::TargetHandler>(
      protocol::TargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      auto_attacher_.get(), session);
  session->CreateAndAddHandler<protocol::NetworkHandler>(
      GetId(), devtools_worker_token(), GetIOContext(), base::DoNothing(),
      session->GetClient());
  return true;
}

protocol::TargetAutoAttacher*
DedicatedWorkerDevToolsAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

std::optional<network::CrossOriginEmbedderPolicy>
DedicatedWorkerDevToolsAgentHost::cross_origin_embedder_policy(
    const std::string&) {
  DedicatedWorkerHost* const host = GetDedicatedWorkerHost();
  return host ? std::make_optional(host->cross_origin_embedder_policy())
              : std::nullopt;
}

}  // namespace content
