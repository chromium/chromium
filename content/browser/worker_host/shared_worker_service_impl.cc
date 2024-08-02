// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_service_impl.h"

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <string>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/renderer_host/private_network_access_util.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/worker_script_fetcher.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/shared_worker_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/script/script_type.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_client.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

SharedWorkerServiceImpl::SharedWorkerServiceImpl(
    StoragePartitionImpl* storage_partition,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : storage_partition_(storage_partition),
      service_worker_context_(std::move(service_worker_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SharedWorkerServiceImpl::~SharedWorkerServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Note: This ideally should dchecks that |worker_hosts_| is empty,
  // but some tests do not tear down everything correctly.
  worker_hosts_.clear();
}

void SharedWorkerServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SharedWorkerServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SharedWorkerServiceImpl::EnumerateSharedWorkers(Observer* observer) {
  for (const auto& host : worker_hosts_) {
    observer->OnWorkerCreated(host->token(), host->GetProcessHost()->GetID(),
                              host->instance().storage_key().origin(),
                              host->GetDevToolsToken());
    if (host->started()) {
      observer->OnFinalResponseURLDetermined(host->token(),
                                             host->final_response_url());
    }
  }
}

bool SharedWorkerServiceImpl::TerminateWorker(
    const GURL& url,
    const std::string& name,
    const blink::StorageKey& storage_key,
    const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SharedWorkerHost* worker_host =
      FindMatchingSharedWorkerHost(url, name, storage_key, same_site_cookies);
  if (worker_host) {
    DestroyHost(worker_host);
    return true;
  }

  return false;
}

void SharedWorkerServiceImpl::Shutdown() {
  worker_hosts_.clear();
  shared_worker_hosts_.clear();
}

void SharedWorkerServiceImpl::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_override_ = std::move(url_loader_factory);
}

void SharedWorkerServiceImpl::ConnectToWorker(
    GlobalRenderFrameHostId client_render_frame_host_id,
    blink::mojom::SharedWorkerInfoPtr info,
    mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    const blink::MessagePortChannel& message_port,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    ukm::SourceId client_ukm_source_id,
    const std::optional<blink::StorageKey>& storage_key_override) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(client_render_frame_host_id);
  if (!render_frame_host) {
    // TODO(crbug.com/40340498): Support the case where the requester is a
    // worker (i.e., nested worker).
    ScriptLoadFailed(std::move(client), /*error_message=*/"");
    return;
  }

  // We always use the render_frame_host storage key here as it doesn't matter
  // if the storage key has been overridden, kAll access is always denied in
  // third-party contexts.
  if (render_frame_host->GetStorageKey().IsThirdPartyContext() &&
      info->same_site_cookies !=
          blink::mojom::SharedWorkerSameSiteCookies::kNone) {
    // Only first-party contexts can request SameSite Strict and Lax cookies.
    ScriptLoadFailed(std::move(client), /*error_message=*/"");
    return;
  }

  // If we are overriding the storage key it must be to a first-party context
  // version of the storage key in the `render_frame_host`.
  CHECK(!storage_key_override ||
        (storage_key_override->IsFirstPartyContext() &&
         (storage_key_override->origin() ==
          render_frame_host->GetStorageKey().origin())));
  const blink::StorageKey& storage_key =
      storage_key_override.value_or(render_frame_host->GetStorageKey());

  // Enforce same-origin policy.
  // data: URLs are not considered a different origin.
  bool is_cross_origin = !info->url.SchemeIs(url::kDataScheme) &&
                         url::Origin::Create(info->url) != storage_key.origin();
  if (is_cross_origin &&
      !GetContentClient()->browser()->DoesSchemeAllowCrossOriginSharedWorker(
          storage_key.origin().scheme())) {
    ScriptLoadFailed(std::move(client), /*error_message=*/"");
    return;
  }

  RenderFrameHost* main_frame = render_frame_host->frame_tree()->GetMainFrame();
  if (!GetContentClient()->browser()->AllowSharedWorker(
          info->url, render_frame_host->ComputeSiteForCookies(),
          main_frame->GetLastCommittedOrigin(), info->options->name,
          storage_key, info->same_site_cookies,
          render_frame_host->GetBrowserContext(),
          client_render_frame_host_id.child_id,
          client_render_frame_host_id.frame_routing_id)) {
    ScriptLoadFailed(std::move(client), /*error_message=*/"");
    return;
  }

  SharedWorkerHost* host = FindMatchingSharedWorkerHost(
      info->url, info->options->name, storage_key, info->same_site_cookies);
  if (host) {
    // Non-secure contexts cannot connect to secure workers, and secure contexts
    // cannot connect to non-secure workers:
    if (host->instance().creation_context_type() != creation_context_type) {
      ScriptLoadFailed(std::move(client), /*error_message=*/"");
      return;
    }
    // Step 11.4: "If worker global scope is not null, then check if worker
    // global scope's type and credentials match the options values. If not,
    // queue a task to fire an event named error and abort these steps."
    // https://html.spec.whatwg.org/C/#dom-sharedworker
    if (host->instance().script_type() != info->options->type ||
        host->instance().credentials_mode() != info->options->credentials) {
      ScriptLoadFailed(
          std::move(client),
          "Failed to connect an existing shared worker because the type or "
          "credentials given on the SharedWorker constructor doesn't match "
          "the existing shared worker's type or credentials.");
      return;
    }

    host->AddClient(std::move(client), client_render_frame_host_id,
                    message_port, client_ukm_source_id);
    return;
  }

  // Could not find an existing SharedWorkerHost to reuse. Create a new one.

  // Get a storage domain.
  auto* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance) {
    ScriptLoadFailed(std::move(client), /*error_message=*/"");
    return;
  }
  auto partition_domain = site_instance->GetPartitionDomain(storage_partition_);
  SharedWorkerInstance instance(info->url, info->options->type,
                                info->options->credentials, info->options->name,
                                storage_key, creation_context_type,
                                info->same_site_cookies);
  host = CreateWorker(
      *render_frame_host, instance, std::move(info->content_security_policies),
      std::move(info->outside_fetch_client_settings_object), partition_domain,
      message_port, std::move(blob_url_loader_factory), storage_key_override);
  if (!host) {
    ScriptLoadFailed(std::move(client), /*error_message=*/"");
    return;
  }
  host->AddClient(std::move(client), client_render_frame_host_id, message_port,
                  client_ukm_source_id);
}

SharedWorkerHost* SharedWorkerServiceImpl::GetSharedWorkerHostFromToken(
    const blink::SharedWorkerToken& worker_token) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = shared_worker_hosts_.find(worker_token);
  if (it == shared_worker_hosts_.end())
    return nullptr;
  return it->second;
}

void SharedWorkerServiceImpl::DestroyHost(SharedWorkerHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(host);
  shared_worker_hosts_.erase(host->token());
  worker_hosts_.erase(worker_hosts_.find(host));
}

void SharedWorkerServiceImpl::NotifyWorkerCreated(
    const blink::SharedWorkerToken& token,
    int worker_process_id,
    const url::Origin& security_origin,
    const base::UnguessableToken& dev_tools_token) {
  for (Observer& observer : observers_) {
    observer.OnWorkerCreated(token, worker_process_id, security_origin,
                             dev_tools_token);
  }
}

void SharedWorkerServiceImpl::NotifyBeforeWorkerDestroyed(
    const blink::SharedWorkerToken& token) {
  for (Observer& observer : observers_)
    observer.OnBeforeWorkerDestroyed(token);
}

void SharedWorkerServiceImpl::NotifyClientAdded(
    const blink::SharedWorkerToken& token,
    GlobalRenderFrameHostId client_render_frame_host_id) {
  auto insertion_result = shared_worker_client_counts_.insert(
      {{token, client_render_frame_host_id}, 0});

  int& count = insertion_result.first->second;
  ++count;

  // Only notify if this is the first time that this frame connects to that
  // shared worker.
  if (insertion_result.second) {
    for (Observer& observer : observers_)
      observer.OnClientAdded(token, client_render_frame_host_id);
  }
}

void SharedWorkerServiceImpl::NotifyClientRemoved(
    const blink::SharedWorkerToken& token,
    GlobalRenderFrameHostId client_render_frame_host_id) {
  auto it = shared_worker_client_counts_.find(
      std::make_pair(token, client_render_frame_host_id));
  CHECK(it != shared_worker_client_counts_.end(), base::NotFatalUntil::M130);

  int& count = it->second;
  DCHECK_GT(count, 0);
  --count;

  // Only notify if there are no longer any active connections from this frame
  // to that shared worker.
  if (count == 0) {
    shared_worker_client_counts_.erase(it);
    for (Observer& observer : observers_)
      observer.OnClientRemoved(token, client_render_frame_host_id);
  }
}

SharedWorkerHost* SharedWorkerServiceImpl::CreateWorker(
    RenderFrameHostImpl& creator,
    const SharedWorkerInstance& instance,
    std::vector<network::mojom::ContentSecurityPolicyPtr>
        content_security_policies,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    const std::string& storage_domain,
    const blink::MessagePortChannel& message_port,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    const std::optional<blink::StorageKey>& storage_key_override) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!blob_url_loader_factory || instance.url().SchemeIsBlob());

  StoragePartitionImpl* partition =
      static_cast<StoragePartitionImpl*>(creator.GetStoragePartition());

  // Use the `creator`'s SiteInstance by default, but if that SiteInstance is
  // cross-origin-isolated, create a new non-isolated SiteInstance for the
  // worker. This is because we have to assume the worker is non-isolated
  // because we don't know its COEP header.
  //
  // TODO(crbug.com/40122193): Move process allocation to after the
  // script is loaded so that the process allocation can take COEP header into
  // account.
  scoped_refptr<SiteInstanceImpl> site_instance = creator.GetSiteInstance();
  if (site_instance->IsCrossOriginIsolated()) {
    site_instance = SiteInstanceImpl::CreateForUrlInfo(
        partition->browser_context(),
        UrlInfo(UrlInfoInit(instance.url())
                    .WithStoragePartitionConfig(partition->GetConfig())
                    .WithWebExposedIsolationInfo(
                        WebExposedIsolationInfo::CreateNonIsolated())),
        partition->is_guest(), site_instance->GetIsolationContext().is_fenced(),
        site_instance->IsFixedStoragePartition());
  }

  RenderProcessHost* worker_process_host = site_instance->GetProcess();
  DCHECK(worker_process_host);
  DCHECK(worker_process_host->InSameStoragePartition(partition));

  if (!worker_process_host->Init()) {
    DVLOG(1) << "Couldn't start a new process for shared worker.";
    return nullptr;
  }

  // Create the host. We need to do this even before starting the worker,
  // because we are about to bounce to the IO thread. If another ConnectToWorker
  // request arrives in the meantime, it finds and reuses the host instead of
  // creating a new host and therefore new SharedWorker thread.
  auto insertion_result =
      worker_hosts_.insert(std::make_unique<SharedWorkerHost>(
          this, instance, std::move(site_instance),
          std::move(content_security_policies),
          creator.policy_container_host()->Clone()));
  DCHECK(insertion_result.second);
  SharedWorkerHost* host = insertion_result.first->get();
  shared_worker_hosts_[host->token()] = host;

  auto service_worker_handle =
      std::make_unique<ServiceWorkerMainResourceHandle>(
          storage_partition_->GetServiceWorkerContext(), base::DoNothing(),
          creator.GetLastCommittedServiceWorkerClient());
  auto* service_worker_handle_raw = service_worker_handle.get();
  host->SetServiceWorkerHandle(std::move(service_worker_handle));

  // Set the credentials mode for worker script fetch based on the script type
  // For classic worker scripts, always use "same-origin" credentials mode.
  // https://html.spec.whatwg.org/C/#fetch-a-classic-worker-script
  // For module worker script, use the credentials mode on WorkerOptions.
  // https://html.spec.whatwg.org/C/#fetch-a-module-worker-script-tree
  const auto credentials_mode =
      instance.script_type() == blink::mojom::ScriptType::kClassic
          ? network::mojom::CredentialsMode::kSameOrigin
          : instance.credentials_mode();

  url::Origin worker_origin = url::Origin::Create(host->instance().url());

  base::WeakPtr<SharedWorkerHost> weak_host = host->AsWeakPtr();
  // Cloning before std::move() so that the object can be used in two functions.
  auto cloned_outside_fetch_client_settings_object =
      outside_fetch_client_settings_object.Clone();

  net::StorageAccessApiStatus storage_access_api_status =
      storage_key_override.has_value()
          ? net::StorageAccessApiStatus::kAccessViaAPI
          : net::StorageAccessApiStatus::kNone;

  // TODO(mmenke): The site-for-cookies and NetworkAnonymizationKey arguments
  // leak data across NetworkIsolationKeys and allow same-site cookies to be
  // sent in cross-site contexts. Fix this. Also, we should probably use
  // `host->instance().storage_key().origin()` instead of `worker_origin`, see
  // following DCHECK.
  DCHECK(host->instance().url().SchemeIs(url::kDataScheme) ||
         GetContentClient()->browser()->DoesSchemeAllowCrossOriginSharedWorker(
             host->instance().storage_key().origin().scheme()) ||
         worker_origin == host->instance().storage_key().origin())
      << worker_origin << " and " << host->instance().storage_key().origin()
      << " should be the same.";
  WorkerScriptFetcher::CreateAndStart(
      worker_process_host->GetID(), host->token(), host->instance().url(),
      creator, &creator,
      host->instance().DoesRequireCrossSiteRequestForCookies()
          ? net::SiteForCookies()
          : host->instance().storage_key().ToNetSiteForCookies(),
      host->instance().storage_key().origin(), host->instance().storage_key(),
      host->instance().storage_key().ToPartialNetIsolationInfo(),
      creator.BuildClientSecurityStateForWorkers(), credentials_mode,
      std::move(outside_fetch_client_settings_object),
      network::mojom::RequestDestination::kSharedWorker,
      service_worker_context_, service_worker_handle_raw,
      std::move(blob_url_loader_factory), url_loader_factory_override_,
      storage_partition_, storage_domain,
      SharedWorkerDevToolsAgentHost::GetFor(host), host->GetDevToolsToken(),
      host->instance().DoesRequireCrossSiteRequestForCookies(),
      storage_access_api_status,
      base::BindOnce(&SharedWorkerServiceImpl::StartWorker,
                     weak_factory_.GetWeakPtr(), weak_host, message_port,
                     std::move(cloned_outside_fetch_client_settings_object)));

  // Ensures that WorkerScriptFetcher::CreateAndStart() doesn't synchronously
  // destroy the SharedWorkerHost.
  DCHECK(weak_host);

  return host;
}

void SharedWorkerServiceImpl::StartWorker(
    base::WeakPtr<SharedWorkerHost> host,
    const blink::MessagePortChannel& message_port,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    std::optional<WorkerScriptFetcherResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The host may already be gone if something forcibly terminated the worker
  // before it could start (e.g., in tests, a UI action or the renderer process
  // is gone). Just fail.
  if (!host)
    return;

  // If the script fetcher failed to load the shared worker's main script,
  // terminate the worker.
  if (!result) {
    DestroyHost(host.get());
    return;
  }

  // Also drop this request if the worker has no clients. This is possible if
  // the frame that caused this worker to be created was deleted between the
  // CreateWorker() and StartWorker() calls. Doing so avoids starting a shared
  // worker and immediately stopping it because its sole client is already being
  // torn down.
  host->PruneNonExistentClients();
  if (!host->HasClients()) {
    DestroyHost(host.get());
    return;
  }

  // TODO(crbug.com/41471904): Check if the main script's final response
  // URL is committable.

  // Get the factory used to instantiate the new shared worker instance in
  // the target process.
  mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory;
  host->GetProcessHost()->BindReceiver(
      factory.InitWithNewPipeAndPassReceiver());

  const GURL final_response_url = result->final_response_url;
  host->Start(std::move(factory),
              std::move(outside_fetch_client_settings_object),
              GetContentClient()->browser(), std::move(*result));
  for (Observer& observer : observers_) {
    observer.OnFinalResponseURLDetermined(host->token(), final_response_url);
  }
}

SharedWorkerHost* SharedWorkerServiceImpl::FindMatchingSharedWorkerHost(
    const GURL& url,
    const std::string& name,
    const blink::StorageKey& storage_key,
    const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies) {
  for (auto& host : worker_hosts_) {
    if (host->instance().Matches(url, name, storage_key, same_site_cookies)) {
      return host.get();
    }
  }
  return nullptr;
}

void SharedWorkerServiceImpl::ScriptLoadFailed(
    mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
    const std::string& error_message) {
  mojo::Remote<blink::mojom::SharedWorkerClient> remote_client(
      std::move(client));
  remote_client->OnScriptLoadFailed(error_message);
}

}  // namespace content
