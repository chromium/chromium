// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/worker_devtools_agent_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/public/browser/child_process_host.h"

namespace content {

namespace protocol {
class TargetAutoAttacher;
}  // namespace protocol

// static
WorkerDevToolsAgentHost* WorkerDevToolsAgentHost::GetFor(
    DedicatedWorkerHost* host) {
  return WorkerDevToolsManager::GetInstance().GetDevToolsHost(host);
}

WorkerDevToolsAgentHost::WorkerDevToolsAgentHost(
    int process_id,
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& devtools_worker_token,
    const std::string& parent_id,
    base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback)
    : DevToolsAgentHostImpl(devtools_worker_token.ToString()),
      process_id_(process_id),
      url_(url),
      name_(name),
      parent_id_(parent_id),
      auto_attacher_(std::make_unique<protocol::RendererAutoAttacherBase>(
          GetRendererChannel())),
      destroyed_callback_(std::move(destroyed_callback)),
      devtools_worker_token_(devtools_worker_token) {
  DCHECK(!devtools_worker_token.is_empty());
  // TODO(crbug.com/906991): Remove AddRef() and Release() once
  // PlzDedicatedWorker is enabled and the code for non-PlzDedicatedWorker is
  // deleted. Worker agent hosts will be retained by the Worker DevTools manager
  // instead.
  AddRef();  // Self keep-alive while the worker agent is alive.
  NotifyCreated();

  if (!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker))
    SetRenderer(process_id, std::move(agent_remote), std::move(host_receiver));
}

WorkerDevToolsAgentHost::~WorkerDevToolsAgentHost() = default;

void WorkerDevToolsAgentHost::SetRenderer(
    int process_id,
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {
  DCHECK(agent_remote);
  DCHECK(host_receiver);

  base::OnceClosure connection_error = (base::BindOnce(
      &WorkerDevToolsAgentHost::Disconnected, base::Unretained(this)));
  GetRendererChannel()->SetRenderer(std::move(agent_remote),
                                    std::move(host_receiver), process_id,
                                    std::move(connection_error));
}

void WorkerDevToolsAgentHost::ChildWorkerCreated(
    const GURL& url,
    const std::string& name,
    base::OnceCallback<void(DevToolsAgentHostImpl*)> callback) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  url_ = url;
  name_ = name;
  destroyed_callback_ = std::move(callback);
}

void WorkerDevToolsAgentHost::Disconnected() {
  auto retain_this = ForceDetachAllSessionsImpl();
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
  std::move(destroyed_callback_).Run(this);
  Release();  // Matches AddRef() in constructor.
}

BrowserContext* WorkerDevToolsAgentHost::GetBrowserContext() {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  return process ? process->GetBrowserContext() : nullptr;
}

RenderProcessHost* WorkerDevToolsAgentHost::GetProcessHost() {
  return RenderProcessHost::FromID(process_id_);
}

std::string WorkerDevToolsAgentHost::GetType() {
  return kTypeDedicatedWorker;
}

std::string WorkerDevToolsAgentHost::GetTitle() {
  return name_.empty() ? url_.spec() : name_;
}

std::string WorkerDevToolsAgentHost::GetParentId() {
  return parent_id_;
}

GURL WorkerDevToolsAgentHost::GetURL() {
  return url_;
}

bool WorkerDevToolsAgentHost::Activate() {
  return false;
}

void WorkerDevToolsAgentHost::Reload() {}

bool WorkerDevToolsAgentHost::Close() {
  return false;
}

bool WorkerDevToolsAgentHost::AttachSession(DevToolsSession* session,
                                            bool acquire_wake_lock) {
  session->CreateAndAddHandler<protocol::IOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::TargetHandler>(
      protocol::TargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      auto_attacher_.get(), session);
  session->CreateAndAddHandler<protocol::NetworkHandler>(
      GetId(), devtools_worker_token_, GetIOContext(), base::DoNothing(),
      session->GetClient()->MayReadLocalFiles());
  return true;
}

void WorkerDevToolsAgentHost::DetachSession(DevToolsSession* session) {
  // Destroying session automatically detaches in renderer.
}

protocol::TargetAutoAttacher* WorkerDevToolsAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

DedicatedWorkerHost* WorkerDevToolsAgentHost::GetDedicatedWorkerHost() {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(process->GetStoragePartition());
  auto* service = storage_partition_impl->GetDedicatedWorkerService();
  return service->GetDedicatedWorkerHostFromToken(
      blink::DedicatedWorkerToken(devtools_worker_token_));
}

absl::optional<network::CrossOriginEmbedderPolicy>
WorkerDevToolsAgentHost::cross_origin_embedder_policy(const std::string&) {
  DedicatedWorkerHost* host = GetDedicatedWorkerHost();
  if (!host) {
    return absl::nullopt;
  }
  return host->cross_origin_embedder_policy();
}

}  // namespace content
