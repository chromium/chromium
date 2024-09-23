// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_worker_devtools_agent_host.h"

#include <memory>
#include <utility>

#include "content/browser/devtools/devtools_renderer_channel.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/fetch_handler.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/schema_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace content {

// static
SharedWorkerDevToolsAgentHost* SharedWorkerDevToolsAgentHost::GetFor(
    SharedWorkerHost* worker_host) {
  return SharedWorkerDevToolsManager::GetInstance()->GetDevToolsHost(
      worker_host);
}

SharedWorkerDevToolsAgentHost::SharedWorkerDevToolsAgentHost(
    SharedWorkerHost* worker_host,
    const base::UnguessableToken& devtools_worker_token)
    : DevToolsAgentHostImpl(devtools_worker_token.ToString()),
      auto_attacher_(std::make_unique<protocol::RendererAutoAttacherBase>(
          GetRendererChannel())),
      state_(WORKER_NOT_READY),
      worker_host_(worker_host),
      devtools_worker_token_(devtools_worker_token),
      instance_(worker_host->instance()) {
  NotifyCreated();
}

SharedWorkerDevToolsAgentHost::~SharedWorkerDevToolsAgentHost() {
  SharedWorkerDevToolsManager::GetInstance()->AgentHostDestroyed(this);
}

BrowserContext* SharedWorkerDevToolsAgentHost::GetBrowserContext() {
  if (!worker_host_)
    return nullptr;
  return worker_host_->GetProcessHost()->GetBrowserContext();
}

std::string SharedWorkerDevToolsAgentHost::GetType() {
  return kTypeSharedWorker;
}

std::string SharedWorkerDevToolsAgentHost::GetTitle() {
  return instance_.name();
}

GURL SharedWorkerDevToolsAgentHost::GetURL() {
  return instance_.url();
}

blink::StorageKey SharedWorkerDevToolsAgentHost::GetStorageKey() const {
  return instance_.storage_key();
}

bool SharedWorkerDevToolsAgentHost::Activate() {
  return false;
}

void SharedWorkerDevToolsAgentHost::Reload() {
}

bool SharedWorkerDevToolsAgentHost::Close() {
  if (worker_host_)
    worker_host_->Destruct();
  return true;
}

bool SharedWorkerDevToolsAgentHost::AttachSession(DevToolsSession* session,
                                                  bool acquire_wake_lock) {
  session->CreateAndAddHandler<protocol::IOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::InspectorHandler>();
  session->CreateAndAddHandler<protocol::NetworkHandler>(
      GetId(), devtools_worker_token_, GetIOContext(),
      base::BindRepeating([] {}), session->GetClient());
  // TODO(crbug.com/40154954): support pushing updated loader factories down to
  // renderer.
  session->CreateAndAddHandler<protocol::FetchHandler>(
      GetIOContext(),
      base::BindRepeating([](base::OnceClosure cb) { std::move(cb).Run(); }));
  session->CreateAndAddHandler<protocol::SchemaHandler>();
  session->CreateAndAddHandler<protocol::TargetHandler>(
      protocol::TargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      auto_attacher_.get(), session);
  return true;
}

void SharedWorkerDevToolsAgentHost::DetachSession(DevToolsSession* session) {
  // Destroying session automatically detaches in renderer.
}

bool SharedWorkerDevToolsAgentHost::Matches(SharedWorkerHost* worker_host) {
  return instance_.Matches(worker_host->instance().url(),
                           worker_host->instance().name(),
                           worker_host->instance().storage_key(),
                           worker_host->instance().same_site_cookies());
}

void SharedWorkerDevToolsAgentHost::WorkerReadyForInspection(
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
        agent_host_receiver) {
  DCHECK_EQ(WORKER_NOT_READY, state_);
  DCHECK(worker_host_);
  state_ = WORKER_READY;
  GetRendererChannel()->SetRenderer(std::move(agent_remote),
                                    std::move(agent_host_receiver),
                                    worker_host_->GetProcessHost()->GetID());
  for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
    inspector->TargetReloadedAfterCrash();
}

void SharedWorkerDevToolsAgentHost::WorkerRestarted(
    SharedWorkerHost* worker_host) {
  DCHECK_EQ(WORKER_TERMINATED, state_);
  DCHECK(!worker_host_);
  state_ = WORKER_NOT_READY;
  worker_host_ = worker_host;
}

void SharedWorkerDevToolsAgentHost::WorkerDestroyed() {
  DCHECK_NE(WORKER_TERMINATED, state_);
  DCHECK(worker_host_);
  state_ = WORKER_TERMINATED;
  for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
    inspector->TargetCrashed();
  worker_host_ = nullptr;
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
}

DevToolsAgentHostImpl::NetworkLoaderFactoryParamsAndInfo
SharedWorkerDevToolsAgentHost::CreateNetworkFactoryParamsForDevTools() {
  DCHECK(worker_host_);
  return {GetStorageKey().origin(),
          instance_.DoesRequireCrossSiteRequestForCookies()
              ? net::SiteForCookies()
              : net::SiteForCookies::FromUrl(GetURL()),
          worker_host_->CreateNetworkFactoryParamsForSubresources()};
}

RenderProcessHost* SharedWorkerDevToolsAgentHost::GetProcessHost() {
  DCHECK(worker_host_);
  return worker_host_->GetProcessHost();
}

protocol::TargetAutoAttacher* SharedWorkerDevToolsAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

}  // namespace content
