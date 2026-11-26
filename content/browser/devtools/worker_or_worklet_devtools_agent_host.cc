// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/worker_or_worklet_devtools_agent_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/safety_checks.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/features.h"
#include "content/public/browser/child_process_host.h"

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
  CHECK(!devtools_worker_token.is_empty(), base::NotFatalUntil::M159);
  if (auto* rph = RenderProcessHost::FromID(process_id)) {
    process_observation_.Observe(rph);
    // The process is known before the renderer is bound; expose it to the
    // handlers of sessions attached in the meantime.
    GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                      process_id);
  }
  if (base::FeatureList::IsEnabled(
          features::kWorkerOrWorkletAgentDoubleReleaseFix)) {
    self_keepalive_ = this;
  } else {
    AddRef();  // Self keep-alive while the worker agent is alive.
  }
}

WorkerOrWorkletDevToolsAgentHost::~WorkerOrWorkletDevToolsAgentHost() {
  ClearPreservedNetworkConditions();
}

void WorkerOrWorkletDevToolsAgentHost::PreserveNetworkConditions() {
  for (protocol::NetworkHandler* handler :
       protocol::NetworkHandler::ForAgentHost(this)) {
    base::OnceClosure cleanup = handler->TakeNetworkConditionsCleanup();
    if (!cleanup) {
      continue;
    }
    const int id = next_preserved_network_conditions_id_++;
    // `this` owns the subscription, so the callback can't outlive `this`.
    // The subscription removes this entry when `root_session` is disposed, so
    // `root_session` doesn't dangle.
    preserved_network_conditions_.emplace(
        id, PreservedNetworkConditions{
                &handler->root_session(), std::move(cleanup),
                handler->root_session().RegisterDisposeCallback(
                    base::BindOnce(&WorkerOrWorkletDevToolsAgentHost::
                                       ClearPreservedNetworkConditionsOfClient,
                                   base::Unretained(this), id))});
  }
}

void WorkerOrWorkletDevToolsAgentHost::ClearPreservedNetworkConditions() {
  std::map<int, PreservedNetworkConditions> preserved;
  preserved.swap(preserved_network_conditions_);
  for (auto& [id, conditions] : preserved) {
    std::move(conditions.cleanup).Run();
  }
}

void WorkerOrWorkletDevToolsAgentHost::
    ClearPreservedNetworkConditionsForRootSession(
        DevToolsSession* root_session) {
  std::vector<base::OnceClosure> cleanups;
  for (auto it = preserved_network_conditions_.begin();
       it != preserved_network_conditions_.end();) {
    if (it->second.root_session == root_session) {
      cleanups.push_back(std::move(it->second.cleanup));
      it = preserved_network_conditions_.erase(it);
    } else {
      ++it;
    }
  }
  for (base::OnceClosure& cleanup : cleanups) {
    std::move(cleanup).Run();
  }
}

void WorkerOrWorkletDevToolsAgentHost::ClearPreservedNetworkConditionsOfClient(
    int id) {
  auto node = preserved_network_conditions_.extract(id);
  if (node) {
    std::move(node.mapped().cleanup).Run();
  }
}

void WorkerOrWorkletDevToolsAgentHost::SetRenderer(
    int process_id,
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {
  CHECK(agent_remote, base::NotFatalUntil::M159);
  CHECK(host_receiver, base::NotFatalUntil::M159);

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
  url_ = url;
  name_ = name;
  destroyed_callback_ = std::move(callback);
}

void WorkerOrWorkletDevToolsAgentHost::Disconnected() {
  // This function is known to be heap allocation heavy and performance
  // critical. Extra memory safety checks can introduce regression
  // (https://crbug.com/414710225) and these are disabled here.
  base::ScopedSafetyChecksExclusion scoped_unsafe;

  auto retain_this = ForceDetachAllSessionsImpl();
  // The worker is gone, so the conditions that outlived its sessions have no
  // more requests to apply to.
  ClearPreservedNetworkConditions();
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);

  if (base::FeatureList::IsEnabled(
          features::kWorkerOrWorkletAgentDoubleReleaseFix)) {
    if (destroyed_callback_) {
      std::move(destroyed_callback_).Run(this);
    }
    process_observation_.Reset();
    self_keepalive_.reset();
  } else {
    std::move(destroyed_callback_).Run(this);
    process_observation_.Reset();
    Release();
  }
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

void WorkerOrWorkletDevToolsAgentHost::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  Disconnected();
}

}  // namespace content
