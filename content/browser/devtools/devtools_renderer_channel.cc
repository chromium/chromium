// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_renderer_channel.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/devtools/dedicated_worker_devtools_agent_host.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/devtools/worklet_devtools_agent_host.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/geometry/point.h"

namespace content {

DevToolsRendererChannel::DevToolsRendererChannel(DevToolsAgentHostImpl* owner)
    : owner_(owner),
      process_id_(ChildProcessHost::kInvalidUniqueID) {}

DevToolsRendererChannel::~DevToolsRendererChannel() = default;

void DevToolsRendererChannel::SetRenderer(
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver,
    int process_id,
    base::OnceClosure connection_error) {
  CleanupConnection();
  blink::mojom::DevToolsAgent* agent = nullptr;
  if (agent_remote.is_valid()) {
    agent_remote_.Bind(std::move(agent_remote));
    agent = agent_remote_.get();
  }
  if (connection_error)
    agent_remote_.set_disconnect_handler(std::move(connection_error));
  if (host_receiver)
    receiver_.Bind(std::move(host_receiver));
  const bool force_using_io = true;
  SetRendererInternal(agent, process_id, nullptr, force_using_io);
}

void DevToolsRendererChannel::SetRendererAssociated(
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgentHost>
        host_receiver,
    int process_id,
    RenderFrameHostImpl* frame_host) {
  CleanupConnection();
  blink::mojom::DevToolsAgent* agent = nullptr;
  if (agent_remote.is_valid()) {
    associated_agent_remote_.Bind(std::move(agent_remote));
    agent = associated_agent_remote_.get();
  }
  if (host_receiver)
    associated_receiver_.Bind(std::move(host_receiver));
  const bool force_using_io = false;
  SetRendererInternal(agent, process_id, frame_host, force_using_io);
}

void DevToolsRendererChannel::CleanupConnection() {
  receiver_.reset();
  associated_receiver_.reset();
  associated_agent_remote_.reset();
  agent_remote_.reset();
}

void DevToolsRendererChannel::ForceDetachWorkerSessions() {
  for (WorkerOrWorkletDevToolsAgentHost* host : child_targets_) {
    host->ForceDetachAllSessions();
  }
}

void DevToolsRendererChannel::SetRendererInternal(
    blink::mojom::DevToolsAgent* agent,
    int process_id,
    RenderFrameHostImpl* frame_host,
    bool force_using_io) {
  ReportChildTargetsCallback();
  process_id_ = process_id;
  frame_host_ = frame_host;
  if (agent && child_target_created_callback_) {
    agent->ReportChildTargets(true /* report */, wait_for_debugger_,
                              base::DoNothing());
  }
  for (DevToolsSession* session : owner_->sessions()) {
    for (auto& pair : session->handlers())
      pair.second->SetRenderer(process_id_, frame_host_);
    session->AttachToAgent(agent, force_using_io);
  }
}

void DevToolsRendererChannel::AttachSession(DevToolsSession* session) {
  if (!agent_remote_ && !associated_agent_remote_)
    owner_->UpdateRendererChannel(true /* force */);
  for (auto& pair : session->handlers())
    pair.second->SetRenderer(process_id_, frame_host_);
  if (agent_remote_)
    session->AttachToAgent(agent_remote_.get(), true);
  else if (associated_agent_remote_)
    session->AttachToAgent(associated_agent_remote_.get(), false);
}

void DevToolsRendererChannel::InspectElement(const gfx::Point& point) {
  if (!agent_remote_ && !associated_agent_remote_)
    owner_->UpdateRendererChannel(true /* force */);
  // Previous call might update |agent_remote_| or |associated_agent_remote_|
  // via SetRenderer(), so we should check them again.
  if (agent_remote_)
    agent_remote_->InspectElement(point);
  else if (associated_agent_remote_)
    associated_agent_remote_->InspectElement(point);
}

void DevToolsRendererChannel::SetReportChildTargets(
    ChildTargetCreatedCallback report_callback,
    bool wait_for_debugger,
    base::OnceClosure completion_callback) {
  DCHECK(report_callback || !wait_for_debugger);
  ReportChildTargetsCallback();
  set_report_completion_callback_ = std::move(completion_callback);

  if (child_target_created_callback_ == report_callback &&
      wait_for_debugger_ == wait_for_debugger) {
    ReportChildTargetsCallback();
    return;
  }
  if (report_callback) {
    for (DevToolsAgentHostImpl* host : child_targets_)
      report_callback.Run(host, false /* waiting_for_debugger */);
  }
  child_target_created_callback_ = std::move(report_callback);
  wait_for_debugger_ = wait_for_debugger;
  if (agent_remote_) {
    agent_remote_->ReportChildTargets(
        !!child_target_created_callback_, wait_for_debugger_,
        base::BindOnce(&DevToolsRendererChannel::ReportChildTargetsCallback,
                       base::Unretained(this)));
  } else if (associated_agent_remote_) {
    associated_agent_remote_->ReportChildTargets(
        !!child_target_created_callback_, wait_for_debugger_,
        base::BindOnce(&DevToolsRendererChannel::ReportChildTargetsCallback,
                       base::Unretained(this)));
  } else {
    ReportChildTargetsCallback();
  }
}

void DevToolsRendererChannel::ReportChildTargetsCallback() {
  if (set_report_completion_callback_)
    std::move(set_report_completion_callback_).Run();
}

void DevToolsRendererChannel::ChildTargetCreated(
    mojo::PendingRemote<blink::mojom::DevToolsAgent> worker_devtools_agent,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& devtools_worker_token,
    bool waiting_for_debugger,
    blink::mojom::DevToolsExecutionContextType context_type) {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  if (!process)
    return;

  GURL filtered_url = url;
  process->FilterURL(true /* empty_allowed */, &filtered_url);

  if (context_type ==
          blink::mojom::DevToolsExecutionContextType::kDedicatedWorker &&
      base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    // WorkerDevToolsAgentHost for dedicated workers is already created on the
    // browser process when PlzDedicatedWorker is enabled.
    DCHECK(
        content::DevToolsAgentHost::GetForId(devtools_worker_token.ToString()));
    scoped_refptr<DedicatedWorkerDevToolsAgentHost> agent_host =
        WorkerDevToolsManager::GetInstance().GetDevToolsHostFromToken(
            devtools_worker_token);
    if (!agent_host) {
      // If `agent_host` is nullptr, we can assume that `DedicatedWorkerHost`
      // has been destructed while handling `DedicatedWorker::ContinueStart`.
      // We do not need to continue in that case.
      return;
    }
    agent_host->ChildWorkerCreated(
        url, name,
        base::BindOnce(&DevToolsRendererChannel::ChildTargetDestroyed,
                       weak_factory_.GetWeakPtr()));
    agent_host->SetRenderer(process_id_, std::move(worker_devtools_agent),
                            std::move(host_receiver));

    child_targets_.insert(agent_host.get());
    if (child_target_created_callback_) {
      child_target_created_callback_.Run(agent_host.get(),
                                         waiting_for_debugger);
    }
    return;
  }

  if (content::DevToolsAgentHost::GetForId(devtools_worker_token.ToString())) {
    mojo::ReportBadMessage("Workers should have unique tokens.");
    return;
  }
  scoped_refptr<WorkerOrWorkletDevToolsAgentHost> agent_host;
  switch (context_type) {
    case blink::mojom::DevToolsExecutionContextType::kWorklet:
      agent_host = base::MakeRefCounted<WorkletDevToolsAgentHost>(
          process_id_, filtered_url, std::move(name), devtools_worker_token,
          owner_->GetId(),
          base::BindOnce(&DevToolsRendererChannel::ChildTargetDestroyed,
                         weak_factory_.GetWeakPtr()));
      break;
    case blink::mojom::DevToolsExecutionContextType::kDedicatedWorker:
      CHECK(
          !base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
      agent_host = base::MakeRefCounted<DedicatedWorkerDevToolsAgentHost>(
          process_id_, filtered_url, std::move(name), devtools_worker_token,
          owner_->GetId(),
          base::BindOnce(&DevToolsRendererChannel::ChildTargetDestroyed,
                         weak_factory_.GetWeakPtr()));
  }
  agent_host->SetRenderer(process_id_, std::move(worker_devtools_agent),
                          std::move(host_receiver));
  child_targets_.insert(agent_host.get());
  if (child_target_created_callback_)
    child_target_created_callback_.Run(agent_host.get(), waiting_for_debugger);
}

void DevToolsRendererChannel::ChildTargetDestroyed(
    DevToolsAgentHostImpl* host) {
  child_targets_.erase(host);
}

void DevToolsRendererChannel::MainThreadDebuggerPaused() {
  owner_->MainThreadDebuggerPaused();
}

void DevToolsRendererChannel::MainThreadDebuggerResumed() {
  owner_->MainThreadDebuggerResumed();
}

void DevToolsRendererChannel::BringToForeground() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate()) {
    manager->delegate()->Activate(owner_);
  }
}

}  // namespace content
