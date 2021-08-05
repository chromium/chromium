// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_renderer_channel.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/worker_devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
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
  for (WorkerDevToolsAgentHost* host : child_workers_)
    host->ForceDetachAllSessions();
}

void DevToolsRendererChannel::SetRendererInternal(
    blink::mojom::DevToolsAgent* agent,
    int process_id,
    RenderFrameHostImpl* frame_host,
    bool force_using_io) {
  ReportChildWorkersCallback();
  process_id_ = process_id;
  frame_host_ = frame_host;
  if (agent && child_worker_created_callback_) {
    agent->ReportChildWorkers(true /* report */, wait_for_debugger_,
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

void DevToolsRendererChannel::SetReportChildWorkers(
    ChildWorkerCreatedCallback report_callback,
    bool wait_for_debugger,
    base::OnceClosure completion_callback) {
  DCHECK(report_callback || !wait_for_debugger);
  ReportChildWorkersCallback();
  set_report_completion_callback_ = std::move(completion_callback);

  if (child_worker_created_callback_ == report_callback &&
      wait_for_debugger_ == wait_for_debugger) {
    ReportChildWorkersCallback();
    return;
  }
  if (report_callback) {
    for (DevToolsAgentHostImpl* host : child_workers_)
      report_callback.Run(host, false /* waiting_for_debugger */);
  }
  child_worker_created_callback_ = std::move(report_callback);
  wait_for_debugger_ = wait_for_debugger;
  if (agent_remote_) {
    agent_remote_->ReportChildWorkers(
        !!child_worker_created_callback_, wait_for_debugger_,
        base::BindOnce(&DevToolsRendererChannel::ReportChildWorkersCallback,
                       base::Unretained(this)));
  } else if (associated_agent_remote_) {
    associated_agent_remote_->ReportChildWorkers(
        !!child_worker_created_callback_, wait_for_debugger_,
        base::BindOnce(&DevToolsRendererChannel::ReportChildWorkersCallback,
                       base::Unretained(this)));
  } else {
    ReportChildWorkersCallback();
  }
}

void DevToolsRendererChannel::ReportChildWorkersCallback() {
  if (set_report_completion_callback_)
    std::move(set_report_completion_callback_).Run();
}

void DevToolsRendererChannel::ChildWorkerCreated(
    mojo::PendingRemote<blink::mojom::DevToolsAgent> worker_devtools_agent,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& devtools_worker_token,
    bool waiting_for_debugger) {
  if (content::DevToolsAgentHost::GetForId(devtools_worker_token.ToString())) {
    mojo::ReportBadMessage("Workers should have unique tokens.");
    return;
  }
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  if (!process)
    return;
  GURL filtered_url = url;
  process->FilterURL(true /* empty_allowed */, &filtered_url);
  auto agent_host = base::MakeRefCounted<WorkerDevToolsAgentHost>(
      process_id_, std::move(worker_devtools_agent), std::move(host_receiver),
      filtered_url, std::move(name), devtools_worker_token, owner_->GetId(),
      base::BindOnce(&DevToolsRendererChannel::ChildWorkerDestroyed,
                     weak_factory_.GetWeakPtr()));
  child_workers_.insert(agent_host.get());
  if (child_worker_created_callback_)
    child_worker_created_callback_.Run(agent_host.get(), waiting_for_debugger);
}

void DevToolsRendererChannel::ChildWorkerDestroyed(
    DevToolsAgentHostImpl* host) {
  child_workers_.erase(host);
}

}  // namespace content
