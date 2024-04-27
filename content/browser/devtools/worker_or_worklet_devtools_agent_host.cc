// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/worker_or_worklet_devtools_agent_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/child_process_host.h"
#include "third_party/blink/public/common/features.h"

namespace content {

WorkerOrWorkletDevToolsAgentHost::WorkerOrWorkletDevToolsAgentHost(
    int process_id,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& devtools_worker_token,
    const std::string& parent_id,
    base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback)
    : DevToolsAgentHostImpl(devtools_worker_token.ToString()),
      devtools_worker_token_(devtools_worker_token),
      parent_id_(parent_id),
      process_id_(process_id),
      url_(url),
      name_(name),
      destroyed_callback_(std::move(destroyed_callback)) {
  DCHECK(!devtools_worker_token.is_empty());
  // TODO(crbug.com/40093136): Remove AddRef() and Release() once
  // PlzDedicatedWorker is enabled and the code for non-PlzDedicatedWorker is
  // deleted. Worker agent hosts will be retained by the Worker DevTools manager
  // instead.
  AddRef();  // Self keep-alive while the worker agent is alive.
}

WorkerOrWorkletDevToolsAgentHost::~WorkerOrWorkletDevToolsAgentHost() = default;

void WorkerOrWorkletDevToolsAgentHost::SetRenderer(
    int process_id,
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {
  DCHECK(agent_remote);
  DCHECK(host_receiver);

  base::OnceClosure connection_error = (base::BindOnce(
      &WorkerOrWorkletDevToolsAgentHost::Disconnected, base::Unretained(this)));
  GetRendererChannel()->SetRenderer(std::move(agent_remote),
                                    std::move(host_receiver), process_id,
                                    std::move(connection_error));
  ProcessHostChanged();
}

void WorkerOrWorkletDevToolsAgentHost::ChildWorkerCreated(
    const GURL& url,
    const std::string& name,
    base::OnceCallback<void(DevToolsAgentHostImpl*)> callback) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  url_ = url;
  name_ = name;
  destroyed_callback_ = std::move(callback);
}

void WorkerOrWorkletDevToolsAgentHost::Disconnected() {
  auto retain_this = ForceDetachAllSessionsImpl();
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
  std::move(destroyed_callback_).Run(this);
  Release();  // Matches AddRef() in constructor.
}

BrowserContext* WorkerOrWorkletDevToolsAgentHost::GetBrowserContext() {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  return process ? process->GetBrowserContext() : nullptr;
}

RenderProcessHost* WorkerOrWorkletDevToolsAgentHost::GetProcessHost() {
  return RenderProcessHost::FromID(process_id_);
}

std::string WorkerOrWorkletDevToolsAgentHost::GetTitle() {
  return name_.empty() ? url_.spec() : name_;
}

std::string WorkerOrWorkletDevToolsAgentHost::GetParentId() {
  return parent_id_;
}

GURL WorkerOrWorkletDevToolsAgentHost::GetURL() {
  return url_;
}

bool WorkerOrWorkletDevToolsAgentHost::Activate() {
  return false;
}

void WorkerOrWorkletDevToolsAgentHost::Reload() {}

bool WorkerOrWorkletDevToolsAgentHost::Close() {
  return false;
}

}  // namespace content
