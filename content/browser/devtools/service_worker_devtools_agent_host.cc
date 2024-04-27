// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/service_worker_devtools_agent_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/devtools/devtools_renderer_channel.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/devtools/protocol/fetch_handler.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/schema_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace content {

namespace {

/*
 In addition to watching for dedicated workers (as all auto-attachers dealing
 with renderer targets do), the service worker auto-attacher below would also
 watch for the new versions of the same service workers. While these may
 already be covered by the parent page auto-attacher, this is essemtial for
 supporting a client that is only attached to the service worker target.
 Please note that this may result with multiple `AutoAttach()` calls to
 the client, but that's ok, as the client only sends CDP notifications for
 that tatgets it hasn't seen previously.
 Here is an example scenario.

      Client                                    Backend
                                           (SW v1 created)
 Target.getTargets()                   ->
 Target.attachTarget(SW_v1)            ->
 Target.autoAttachRelated(SW_v1)       ->
                                           (SW v2 created)
                                       <-  Target.attachedToTarget(SW_v2)
 Target.autoAttachRelated(SW_v2)       ->
 Runtime.runIfWaitingForDebugger       ->
                                           (SW v1 stopped)
                                           Target.detachedFromTarget(SW_v1)
*/

class ServiceWorkerAutoAttacher
    : public protocol::RendererAutoAttacherBase,
      public ServiceWorkerDevToolsManager::Observer {
 public:
  ServiceWorkerAutoAttacher(DevToolsRendererChannel* renderer_channel,
                            ServiceWorkerDevToolsAgentHost* host)
      : RendererAutoAttacherBase(renderer_channel), host_(host) {}
  ~ServiceWorkerAutoAttacher() override {
    if (have_observer_)
      ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
  }

 private:
  // ServiceWorkerDevToolsManager::Observer implementation.
  void WorkerCreated(ServiceWorkerDevToolsAgentHost* host,
                     bool* should_pause_on_start) override {
    if (!IsNewerVersion(host))
      return;
    *should_pause_on_start = wait_for_debugger_on_start();
    DispatchAutoAttach(host, *should_pause_on_start);
  }

  void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) override {
    // Report an auto-detached service worker for any host with same
    // registration, to provide for the case where its older version that could
    // have had it auto-attached may have been shut down at this point.
    if (MatchRegistration(host))
      DispatchAutoDetach(host);
  }

  void UpdateAutoAttach(base::OnceClosure callback) override {
    bool enabled = auto_attach();
    if (have_observer_ != enabled) {
      if (enabled) {
        ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);
        ServiceWorkerDevToolsAgentHost::List agent_hosts;
        ServiceWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(
            &agent_hosts);
        for (auto& host : agent_hosts) {
          if (IsNewerVersion(host.get()))
            DispatchAutoAttach(host.get(), false);
        }
      } else {
        ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
      }
      have_observer_ = enabled;
    }
    RendererAutoAttacherBase::UpdateAutoAttach(std::move(callback));
  }

  bool MatchRegistration(ServiceWorkerDevToolsAgentHost* their_host) const {
    return host_->context_wrapper() == their_host->context_wrapper() &&
           host_->scope() == their_host->scope() &&
           host_->GetURL() == their_host->GetURL();
  }

  bool IsNewerVersion(ServiceWorkerDevToolsAgentHost* their_host) {
    return MatchRegistration(their_host) &&
           their_host->version_id() > host_->version_id();
  }

  bool have_observer_ = false;
  raw_ptr<ServiceWorkerDevToolsAgentHost> host_;
};

}  // namespace

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForServiceWorker(
    ServiceWorkerContext* context,
    int64_t version_id) {
  auto* context_wrapper = static_cast<ServiceWorkerContextWrapper*>(context);
  ServiceWorkerDevToolsAgentHost::List hosts;
  ServiceWorkerDevToolsManager::GetInstance()
      ->AddAllAgentHostsForBrowserContext(context_wrapper->browser_context(),
                                          &hosts);
  for (auto& host : hosts) {
    if (host->context_wrapper() == context_wrapper &&
        host->version_id() == version_id) {
      return host;
    }
  }
  return nullptr;
}

ServiceWorkerDevToolsAgentHost::ServiceWorkerDevToolsAgentHost(
    int worker_process_id,
    int worker_route_id,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id,
    const GURL& url,
    const GURL& scope,
    bool is_installed_version,
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    const base::UnguessableToken& devtools_worker_token)
    : DevToolsAgentHostImpl(devtools_worker_token.ToString()),
      auto_attacher_(
          std::make_unique<ServiceWorkerAutoAttacher>(GetRendererChannel(),
                                                      this)),
      state_(WORKER_NOT_READY),
      devtools_worker_token_(devtools_worker_token),
      worker_process_id_(worker_process_id),
      worker_route_id_(worker_route_id),
      context_wrapper_(context_wrapper),
      version_id_(version_id),
      url_(url),
      scope_(scope),
      version_installed_time_(is_installed_version ? base::Time::Now()
                                                   : base::Time()),
      client_security_state_(std::move(client_security_state)),
      coep_reporter_(std::move(coep_reporter)) {
  UpdateProcessHost();
  NotifyCreated();
}

BrowserContext* ServiceWorkerDevToolsAgentHost::GetBrowserContext() {
  return context_wrapper_->browser_context();
}

std::string ServiceWorkerDevToolsAgentHost::GetType() {
  return kTypeServiceWorker;
}

std::string ServiceWorkerDevToolsAgentHost::GetTitle() {
  return "Service Worker " + url_.spec();
}

GURL ServiceWorkerDevToolsAgentHost::GetURL() {
  return url_;
}

bool ServiceWorkerDevToolsAgentHost::Activate() {
  return false;
}

void ServiceWorkerDevToolsAgentHost::Reload() {
}

bool ServiceWorkerDevToolsAgentHost::Close() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ServiceWorkerVersion* version =
          context_wrapper_->GetLiveVersion(version_id_)) {
    version->StopWorker(base::DoNothing());
  }

  return true;
}

void ServiceWorkerDevToolsAgentHost::WorkerVersionInstalled() {
  version_installed_time_ = base::Time::Now();
}

void ServiceWorkerDevToolsAgentHost::WorkerVersionDoomed() {
  version_doomed_time_ = base::Time::Now();
}

void ServiceWorkerDevToolsAgentHost::WorkerMainScriptFetchingFailed() {
  for (DevToolsSession* session : sessions())
    session->ClearPendingMessages(/*did_crash=*/false);
}

ServiceWorkerDevToolsAgentHost::~ServiceWorkerDevToolsAgentHost() {
  ServiceWorkerDevToolsManager::GetInstance()->AgentHostDestroyed(this);
}

bool ServiceWorkerDevToolsAgentHost::AttachSession(DevToolsSession* session,
                                                   bool acquire_wake_lock) {
  session->CreateAndAddHandler<protocol::IOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::InspectorHandler>();
  session->CreateAndAddHandler<protocol::NetworkHandler>(
      GetId(), devtools_worker_token_, GetIOContext(), base::DoNothing(),
      session->GetClient());

  session->CreateAndAddHandler<protocol::FetchHandler>(
      GetIOContext(),
      base::BindRepeating(
          &ServiceWorkerDevToolsAgentHost::UpdateLoaderFactories,
          base::Unretained(this)));
  session->CreateAndAddHandler<protocol::SchemaHandler>();

  auto* target_handler = session->CreateAndAddHandler<protocol::TargetHandler>(
      protocol::TargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      auto_attacher_.get(), session);
  DCHECK(target_handler);
  target_handler->DisableAutoAttachOfServiceWorkers();

  if (state_ == WORKER_READY && sessions().empty())
    UpdateIsAttached(true);
  return true;
}

void ServiceWorkerDevToolsAgentHost::DetachSession(DevToolsSession* session) {
  // Destroying session automatically detaches in renderer.
  if (state_ == WORKER_READY && sessions().empty())
    UpdateIsAttached(false);
}

protocol::TargetAutoAttacher* ServiceWorkerDevToolsAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

void ServiceWorkerDevToolsAgentHost::WorkerReadyForInspection(
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {
  DCHECK_EQ(WORKER_NOT_READY, state_);
  state_ = WORKER_READY;
  GetRendererChannel()->SetRenderer(
      std::move(agent_remote), std::move(host_receiver), worker_process_id_);
  for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
    inspector->TargetReloadedAfterCrash();
  if (!sessions().empty())
    UpdateIsAttached(true);
}

void ServiceWorkerDevToolsAgentHost::UpdateClientSecurityState(
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter) {
  DCHECK(client_security_state);
  client_security_state_ = std::move(client_security_state);
  coep_reporter_.Bind(std::move(coep_reporter));
}

void ServiceWorkerDevToolsAgentHost::WorkerStarted(int worker_process_id,
                                                   int worker_route_id) {
  DCHECK(state_ == WORKER_NOT_READY || state_ == WORKER_TERMINATED);
  state_ = WORKER_NOT_READY;
  worker_process_id_ = worker_process_id;
  worker_route_id_ = worker_route_id;
  UpdateProcessHost();
}

void ServiceWorkerDevToolsAgentHost::WorkerStopped() {
  DCHECK_NE(WORKER_TERMINATED, state_);
  state_ = WORKER_TERMINATED;
  worker_process_id_ = content::ChildProcessHost::kInvalidUniqueID;
  worker_route_id_ = MSG_ROUTING_NONE;
  for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
    inspector->TargetCrashed();
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
  if (!sessions().empty())
    UpdateIsAttached(false);
}

void ServiceWorkerDevToolsAgentHost::UpdateIsAttached(bool attached) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (ServiceWorkerVersion* version =
          context_wrapper_->GetLiveVersion(version_id_))
    version->SetDevToolsAttached(attached);
}

void ServiceWorkerDevToolsAgentHost::UpdateProcessHost() {
  process_observation_.Reset();
  if (auto* rph = RenderProcessHost::FromID(worker_process_id_))
    process_observation_.Observe(rph);
}

void ServiceWorkerDevToolsAgentHost::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  scoped_refptr<DevToolsAgentHost> retain_this;
  if (context_wrapper_->process_manager()->IsShutdown())
    retain_this = ForceDetachAllSessionsImpl();
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
  process_observation_.Reset();
}

void ServiceWorkerDevToolsAgentHost::UpdateLoaderFactories(
    base::OnceClosure callback) {
  if (state_ == WORKER_TERMINATED) {
    std::move(callback).Run();
    return;
  }
  RenderProcessHost* rph = RenderProcessHost::FromID(worker_process_id_);
  if (!rph) {
    std::move(callback).Run();
    return;
  }
  const url::Origin origin = url::Origin::Create(url_);

  // There should never be a COEP reporter without a client security state.
  DCHECK(!coep_reporter_ || client_security_state_);

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_for_script_loader;
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_for_subresource_loader;
  if (coep_reporter_) {
    coep_reporter_->Clone(
        coep_reporter_for_script_loader.InitWithNewPipeAndPassReceiver());
    coep_reporter_->Clone(
        coep_reporter_for_subresource_loader.InitWithNewPipeAndPassReceiver());
  }

  auto* version = context_wrapper_->GetLiveVersion(version_id_);
  if (!version) {
    std::move(callback).Run();
    return;
  }

  auto script_bundle = EmbeddedWorkerInstance::CreateFactoryBundle(
      rph, worker_route_id_, version->key(), client_security_state_.Clone(),
      std::move(coep_reporter_for_script_loader),
      ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerScript,
      GetId());
  auto subresource_bundle = EmbeddedWorkerInstance::CreateFactoryBundle(
      rph, worker_route_id_, version->key(), client_security_state_.Clone(),
      std::move(coep_reporter_for_subresource_loader),
      ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerSubResource,
      GetId());

  version->embedded_worker()->UpdateLoaderFactories(
      std::move(script_bundle), std::move(subresource_bundle));

  std::move(callback).Run();
}

DevToolsAgentHostImpl::NetworkLoaderFactoryParamsAndInfo
ServiceWorkerDevToolsAgentHost::CreateNetworkFactoryParamsForDevTools() {
  RenderProcessHost* rph = RenderProcessHost::FromID(worker_process_id_);
  const url::Origin origin = url::Origin::Create(url_);
  const auto* version = context_wrapper_->GetLiveVersion(version_id_);
  // TODO(crbug.com/40190528): make sure client_security_state is no longer
  // nullptr anywhere.
  auto factory = URLLoaderFactoryParamsHelper::CreateForWorker(
      rph, origin, version->key().ToPartialNetIsolationInfo(),
      /*coep_reporter=*/mojo::NullRemote(),
      static_cast<StoragePartitionImpl*>(rph->GetStoragePartition())
          ->CreateAuthCertObserverForServiceWorker(rph->GetID()),
      NetworkServiceDevToolsObserver::MakeSelfOwned(GetId()),
      /*client_security_state=*/nullptr,
      /*debug_tag=*/"SWDTAH::CreateNetworkFactoryParamsForDevTools",
      /*require_cross_site_request_for_cookies=*/false);
  return {url::Origin::Create(GetURL()), net::SiteForCookies::FromUrl(GetURL()),
          std::move(factory)};
}

RenderProcessHost* ServiceWorkerDevToolsAgentHost::GetProcessHost() {
  return RenderProcessHost::FromID(worker_process_id_);
}

std::optional<network::CrossOriginEmbedderPolicy>
ServiceWorkerDevToolsAgentHost::cross_origin_embedder_policy(
    const std::string&) {
  if (!client_security_state_) {
    return std::nullopt;
  }
  return client_security_state_->cross_origin_embedder_policy;
}

void ServiceWorkerDevToolsAgentHost::set_should_pause_on_start(
    bool should_pause_on_start) {
  should_pause_on_start_ = should_pause_on_start;
}

}  // namespace content
