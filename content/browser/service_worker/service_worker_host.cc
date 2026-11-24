// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_host.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/broadcast_channel/broadcast_channel_service.h"
#include "content/browser/buckets/bucket_manager.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/renderer_host/clipboard_host_impl.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webtransport/web_transport_connector_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/site_for_cookies.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "storage/browser/blob/blob_url_store_impl.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

BASE_FEATURE(kScopeServiceWorkerRestrictedCookieManager,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsCookieDomainValid(const GURL& url,
                         const net::CanonicalCookie* cookie,
                         const net::CookieInclusionStatus& status) {
  if (cookie && !cookie->IsDomainMatch(url.GetHost())) {
    return false;
  }
  if (status.HasExclusionReason(net::CookieInclusionStatus::ExclusionReason::
                                    EXCLUDE_DOMAIN_MISMATCH)) {
    return false;
  }
  return true;
}

// Forwards RestrictedCookieManager requests from a service worker to the
// underlying cookie manager while scoping the capability lifetime to the owning
// ServiceWorkerHost.
class ServiceWorkerRestrictedCookieManager final
    : public network::mojom::RestrictedCookieManager {
 public:
  ServiceWorkerRestrictedCookieManager(
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<network::mojom::RestrictedCookieManager> backend,
      mojo::UniqueReceiverSet<network::mojom::RestrictedCookieManager>&
          receiver_set)
      : origin_(origin),
        cookie_partition_key_(net::CookiePartitionKey::FromNetworkIsolationKey(
            isolation_info.network_isolation_key(),
            isolation_info.site_for_cookies(),
            net::SchemefulSite(origin),
            isolation_info.IsMainFrameRequest())),
        backend_(std::move(backend)),
        receiver_set_(receiver_set) {}

  ServiceWorkerRestrictedCookieManager(
      const ServiceWorkerRestrictedCookieManager&) = delete;
  ServiceWorkerRestrictedCookieManager& operator=(
      const ServiceWorkerRestrictedCookieManager&) = delete;

  ~ServiceWorkerRestrictedCookieManager() override = default;

  void SetBackendDisconnectHandler(base::OnceClosure handler) {
    // Propagate upstream disconnection (for example if the network service
    // restarts or closes the backend receiver) by removing the corresponding
    // worker-facing receiver from `ServiceWorkerHost`.
    backend_.set_disconnect_handler(std::move(handler));
  }

  // network::mojom::RestrictedCookieManager:
  void GetAllForUrl(const GURL& url,
                    const net::SiteForCookies& site_for_cookies,
                    const url::Origin& top_frame_origin,
                    net::StorageAccessApiStatus storage_access_api_status,
                    network::mojom::CookieManagerGetOptionsPtr options,
                    bool is_ad_tagged,
                    bool apply_devtools_overrides,
                    bool force_disable_third_party_cookies,
                    GetAllForUrlCallback callback) override {
    if (!ValidateAccessToCookiesAt(url)) {
      std::move(callback).Run({});
      return;
    }
    backend_->GetAllForUrl(
        url, site_for_cookies, top_frame_origin, storage_access_api_status,
        std::move(options), is_ad_tagged, apply_devtools_overrides,
        force_disable_third_party_cookies,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            std::move(callback), std::vector<net::CookieWithAccessResult>()));
  }

  void SetCanonicalCookie(
      network::mojom::RestrictedCanonicalCookieParamsPtr cookie_params,
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      bool is_ad_tagged,
      bool apply_devtools_overrides,
      SetCanonicalCookieCallback callback) override {
    if (!ValidateAccessToCookiesAt(url)) {
      std::move(callback).Run(false);
      return;
    }

    std::optional<net::CookiePartitionKey> cookie_partition_key =
        cookie_params->partitioned ==
                    network::mojom::RestrictedCookiePartition::PARTITIONED ||
                net::CookiePartitionKey::HasNonce(cookie_partition_key_)
            ? cookie_partition_key_
            : std::nullopt;
    net::CookieInclusionStatus status;
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateSanitizedCookie(
            url, cookie_params->name, cookie_params->value,
            cookie_params->domain, cookie_params->path, cookie_params->creation,
            cookie_params->expires, cookie_params->last_access,
            cookie_params->secure, cookie_params->http_only,
            cookie_params->same_site, cookie_params->priority,
            cookie_partition_key, &status);
    if (!IsCookieDomainValid(url, cookie.get(), status)) {
      ReportBadMessage("Setting cookies on other domains is disallowed.");
      std::move(callback).Run(false);
      return;
    }
    if (!cookie) {
      std::move(callback).Run(false);
      return;
    }

    backend_->SetCanonicalCookie(
        std::move(cookie_params), url, site_for_cookies, top_frame_origin,
        storage_access_api_status, is_ad_tagged, apply_devtools_overrides,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                    false));
  }

  void AddChangeListener(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      mojo::PendingRemote<network::mojom::CookieChangeListener> listener,
      AddChangeListenerCallback callback) override {
    if (url.is_empty()) {
      std::move(callback).Run();
      return;
    }
    if (!ValidateAccessToCookiesAt(url)) {
      std::move(callback).Run();
      return;
    }
    backend_->AddChangeListener(
        url, site_for_cookies, top_frame_origin, storage_access_api_status,
        std::move(listener),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback)));
  }

  void SetCookieFromString(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      bool is_ad_tagged,
      bool apply_devtools_overrides,
      const std::string& cookie) override {
    if (!ValidateAccessToCookiesAt(url)) {
      return;
    }
    backend_->SetCookieFromString(url, site_for_cookies, top_frame_origin,
                                  storage_access_api_status, is_ad_tagged,
                                  apply_devtools_overrides, cookie);
  }

  void GetCookiesString(const GURL& url,
                        const net::SiteForCookies& site_for_cookies,
                        const url::Origin& top_frame_origin,
                        net::StorageAccessApiStatus storage_access_api_status,
                        bool get_version_shared_memory,
                        bool is_ad_tagged,
                        bool apply_devtools_overrides,
                        bool force_disable_third_party_cookies,
                        GetCookiesStringCallback callback) override {
    if (!ValidateAccessToCookiesAt(url)) {
      std::move(callback).Run(network::mojom::kInvalidCookieVersion,
                              base::ReadOnlySharedMemoryRegion(),
                              std::string());
      return;
    }
    backend_->GetCookiesString(
        url, site_for_cookies, top_frame_origin, storage_access_api_status,
        get_version_shared_memory, is_ad_tagged, apply_devtools_overrides,
        force_disable_third_party_cookies,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            std::move(callback), network::mojom::kInvalidCookieVersion,
            base::ReadOnlySharedMemoryRegion(), std::string()));
  }

  void CookiesEnabledFor(const GURL& url,
                         const net::SiteForCookies& site_for_cookies,
                         const url::Origin& top_frame_origin,
                         net::StorageAccessApiStatus storage_access_api_status,
                         bool apply_devtools_overrides,
                         CookiesEnabledForCallback callback) override {
    if (!ValidateAccessToCookiesAt(url)) {
      std::move(callback).Run(false);
      return;
    }
    backend_->CookiesEnabledFor(url, site_for_cookies, top_frame_origin,
                                storage_access_api_status,
                                apply_devtools_overrides,
                                mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                                    std::move(callback), false));
  }

 private:
  bool ValidateAccessToCookiesAt(const GURL& url) {
    if (origin_.opaque()) {
      ReportBadMessage("Access is denied in this context");
      return false;
    }
    if (!origin_.IsSameOriginWith(url)) {
      ReportBadMessage("Incorrect url origin");
      return false;
    }
    return true;
  }

  void ReportBadMessage(std::string_view error) {
    mojo::UniqueReceiverSet<network::mojom::RestrictedCookieManager>&
        receiver_set = *receiver_set_;
    // `ReportBadMessage()` synchronously removes the active receiver from
    // `receiver_set` and destroys `this`; do not access `this` afterward.
    receiver_set.ReportBadMessage(error);
  }

  const url::Origin origin_;
  const std::optional<net::CookiePartitionKey> cookie_partition_key_;
  mojo::Remote<network::mojom::RestrictedCookieManager> backend_;
  // Safe because `receiver_set_` (owned by `ServiceWorkerHost`) owns `this`.
  const raw_ref<
      mojo::UniqueReceiverSet<network::mojom::RestrictedCookieManager>>
      receiver_set_;
};

}  // namespace

ServiceWorkerHost::ServiceWorkerHost(
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    ServiceWorkerVersion& version,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : worker_process_id_(ChildProcessHost::kInvalidUniqueID),
      version_(&version),
      token_(blink::ServiceWorkerToken()),
      broker_(this),
      container_host_(
          std::make_unique<content::ServiceWorkerContainerHostForServiceWorker>(
              std::move(context),
              this,
              version_->script_url(),
              version_->key())),
      host_receiver_(container_host_.get(), std::move(host_receiver)) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
}

ServiceWorkerHost::~ServiceWorkerHost() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  // Explicitly destroy the ServiceWorkerContainerHost to release
  // ServiceWorkerObjectHosts and ServiceWorkerRegistrationObjectHosts owned by
  // that. Otherwise, this destructor can trigger their Mojo connection error
  // handlers, which would call back into halfway destroyed |this|. This is
  // because they are associated with the ServiceWorker interface, which can be
  // destroyed while in this destructor (|version_|'s |event_dispatcher_|).
  // See https://crbug.com/854993.
  container_host_.reset();
}

void ServiceWorkerHost::CompleteStartWorkerPreparation(
    ChildProcessId process_id,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> broker_receiver,
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        interface_provider_remote) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  CHECK(!worker_process_id_, base::NotFatalUntil::M159);
  CHECK(process_id, base::NotFatalUntil::M159);
  worker_process_id_ = process_id;
  broker_receiver_.Bind(std::move(broker_receiver));
  remote_interfaces_.Bind(std::move(interface_provider_remote));
}

void ServiceWorkerHost::CreateWebTransportConnector(
    mojo::PendingReceiver<blink::mojom::WebTransportConnector> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  base::UnguessableToken worker_devtools_id =
      version_->embedded_worker()->WorkerDevtoolsId();
  std::optional<base::UnguessableToken> devtools_token =
      worker_devtools_id.is_empty() ? std::nullopt
                                    : std::optional(worker_devtools_id);
  // TODO(crbug.com/379869738): Remove GetUnsafeValue.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebTransportConnectorImpl>(
          worker_process_id_.GetUnsafeValue(), /*frame=*/nullptr,
          WeakDocumentPtr(), version_->key().origin(),
          GetNetworkAnonymizationKey(),
          version_->BuildClientSecurityState()->Clone(),
          version_->network_restrictions_id(), devtools_token),
      std::move(receiver));
}

void ServiceWorkerHost::CreateWebSocketConnector(
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  const blink::StorageKey& storage_key = version_->key();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebSocketConnectorImpl>(
          content::GlobalRenderFrameHostId(worker_process_id_,
                                           IPC::mojom::kRoutingIdNone),
          WeakDocumentPtr(), storage_key.origin(),
          storage_key.ToPartialNetIsolationInfo(),
          version_->BuildClientSecurityState()->Clone(),
          net::StorageAccessApiStatus::kNone,
          version_->network_restrictions_id(),
          version_->embedded_worker()->WorkerDevtoolsId()),
      std::move(receiver));
}

void ServiceWorkerHost::BindCacheStorage(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  version_->embedded_worker()->BindCacheStorage(
      std::move(receiver),
      storage::BucketLocator::ForDefaultBucket(version_->key()));
}

void ServiceWorkerHost::GetSandboxedFileSystemForBucket(
    const storage::BucketInfo& bucket,
    const std::vector<std::string>& directory_path_components,
    blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
        callback) {
  auto* process = GetProcessHost();
  if (process) {
    process->GetSandboxedFileSystemForBucket(bucket.ToBucketLocator(),
                                             directory_path_components,
                                             std::move(callback));
  } else {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kInvalidState,
            "Process gone."),
        {});
  }
}

void ServiceWorkerHost::BindHidService(
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  version_->embedded_worker()->BindHidService(version_->key().origin(),
                                              std::move(receiver));
}

void ServiceWorkerHost::BindClipboardHost(
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!GetContentClient()->browser()->IsClipboardAllowedForServiceWorker(
          version_->key().origin())) {
    // The renderer only exposes navigator.clipboard to eligible workers, so a
    // request from an ineligible one means a compromised renderer.
    mojo::ReportBadMessage(
        "Clipboard is not allowed for this service worker origin.");
    return;
  }
  clipboard_hosts_.Add(std::make_unique<ClipboardHostImpl>(*this),
                       std::move(receiver));
}

void ServiceWorkerHost::BindRestrictedCookieManager(
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  RenderProcessHost* process = RenderProcessHost::FromID(worker_process_id_);
  if (!process) {
    return;
  }

  const blink::StorageKey& storage_key = version_->key();
  if (!base::FeatureList::IsEnabled(
          kScopeServiceWorkerRestrictedCookieManager)) {
    process->BindRestrictedCookieManagerForServiceWorker(storage_key,
                                                         std::move(receiver));
    return;
  }

  mojo::PendingRemote<network::mojom::RestrictedCookieManager> backend_remote;
  process->BindRestrictedCookieManagerForServiceWorker(
      storage_key, backend_remote.InitWithNewPipeAndPassReceiver());

  auto proxy = std::make_unique<ServiceWorkerRestrictedCookieManager>(
      storage_key.origin(), storage_key.ToPartialNetIsolationInfo(),
      std::move(backend_remote), restricted_cookie_managers_);
  ServiceWorkerRestrictedCookieManager* raw_proxy = proxy.get();
  mojo::ReceiverId receiver_id =
      restricted_cookie_managers_.Add(std::move(proxy), std::move(receiver));
  raw_proxy->SetBackendDisconnectHandler(
      base::BindOnce(&ServiceWorkerHost::OnRestrictedCookieManagerDisconnected,
                     weak_factory_.GetWeakPtr(), receiver_id));
}

void ServiceWorkerHost::OnRestrictedCookieManagerDisconnected(
    mojo::ReceiverId receiver_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  restricted_cookie_managers_.Remove(receiver_id);
}

void ServiceWorkerHost::BindUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (container_host_->top_frame_origin().opaque()) {
    // Service worker should not be available to a window/worker client whose
    // origin is opaque according to Service Worker specification. However, this
    // can possibly be triggered by a compromised renderer, so reject it and
    // report a bad mojo message.
    mojo::ReportBadMessage(
        "WebUSB is not allowed for the service worker scope when the top-level "
        "frame has an opaque origin.");
    return;
  }
  version_->embedded_worker()->BindUsbService(
      container_host_->top_frame_origin(), std::move(receiver));
}

net::NetworkIsolationKey ServiceWorkerHost::GetNetworkIsolationKey() const {
  return version_->key().ToPartialNetIsolationInfo().network_isolation_key();
}

net::NetworkAnonymizationKey ServiceWorkerHost::GetNetworkAnonymizationKey()
    const {
  return version_->key()
      .ToPartialNetIsolationInfo()
      .network_anonymization_key();
}

const base::UnguessableToken& ServiceWorkerHost::GetReportingSource() const {
  return version_->reporting_source();
}

StoragePartition* ServiceWorkerHost::GetStoragePartition() const {
  // It is possible that the RenderProcessHost is gone but we receive a request
  // before we had the opportunity to Detach because the disconnect handler
  // wasn't run yet. In such cases it is is safe to ignore these messages since
  // we are about to stop the service worker.
  auto* process = GetProcessHost();

  if (process == nullptr) {
    return nullptr;
  }

  return process->GetStoragePartition();
}

void ServiceWorkerHost::CreateCodeCacheHost(
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver) {
  auto embedded_worker_status = version_->embedded_worker()->status();
  // Due to IPC races it is possible that we receive code cache host requests
  // when the worker is stopping. For ex:
  // 1) Browser starts trying to stop, sends the Stop() IPC.
  // 2) Renderer sends a CreateCodeCacheHost() IPC.
  // 3) Renderer gets the Stop() IPC and realize it should try to stop the
  // worker.
  // Given the worker is stopping it is safe to ignore these messages.
  if (embedded_worker_status == blink::EmbeddedWorkerStatus::kStopping) {
    return;
  }

  // Create a new CodeCacheHostImpl and bind it to the given receiver.
  StoragePartition* storage_partition = GetStoragePartition();
  if (!storage_partition) {
    return;
  }
  if (!code_cache_host_receivers_) {
    code_cache_host_receivers_ =
        std::make_unique<CodeCacheHostImpl::ReceiverSet>(
            storage_partition->GetGeneratedCodeCacheContext());
  }
  code_cache_host_receivers_->Add(version_->embedded_worker()->process_id(),
                                  GetNetworkIsolationKey(),
                                  GetBucketStorageKey(), std::move(receiver));
}

void ServiceWorkerHost::CreateBroadcastChannelProvider(
    mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver) {
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());
  if (!storage_partition_impl) {
    return;
  }

  auto* broadcast_channel_service =
      storage_partition_impl->GetBroadcastChannelService();
  broadcast_channel_service->AddReceiver(
      std::make_unique<BroadcastChannelProvider>(broadcast_channel_service,
                                                 version()->key()),
      std::move(receiver));
}

void ServiceWorkerHost::CreateBlobUrlStoreProvider(
    mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver) {
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());
  if (!storage_partition_impl) {
    return;
  }

  storage_partition_impl->GetBlobUrlRegistry()->AddReceiver(
      version()->key(), version()->key().origin(),
      GetProcessHost()->GetDeprecatedID(), std::move(receiver),
      /*context_type_for_debugging=*/"Service Worker",
      base::BindRepeating(
          [](base::WeakPtr<ServiceWorkerHost> host) -> std::string {
            if (!host) {
              return "destroyed ServiceWorkerHost";
            }
            return host->version()->key().GetDebugString();
          },
          weak_factory_.GetWeakPtr()),
      // Storage access can only be granted to dedicated workers.
      base::BindRepeating([]() -> bool { return false; }),
      !(GetContentClient()->browser()->IsBlobUrlPartitioningEnabled(
          GetProcessHost()->GetBrowserContext())));
}

void ServiceWorkerHost::CreateBucketManagerHost(
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  static_cast<StoragePartitionImpl*>(GetStoragePartition())
      ->GetBucketManager()
      ->BindReceiver(GetWeakPtr(), std::move(receiver),
                     mojo::GetBadMessageCallback());
}

base::WeakPtr<ServiceWorkerHost> ServiceWorkerHost::GetWeakPtr() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  return weak_factory_.GetWeakPtr();
}

void ServiceWorkerHost::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the service worker scope");
}

blink::StorageKey ServiceWorkerHost::GetBucketStorageKey() {
  return version_->key();
}

blink::mojom::PermissionStatus ServiceWorkerHost::GetPermissionStatus(
    blink::PermissionType permission_type) {
  auto* process = GetProcessHost();

  if (!process) {
    return blink::mojom::PermissionStatus::DENIED;
  }

  return process->GetBrowserContext()
      ->GetPermissionController()
      ->GetPermissionStatusForWorker(
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(permission_type),
          process, GetBucketStorageKey().origin());
}

void ServiceWorkerHost::BindCacheStorageForBucket(
    const storage::BucketInfo& bucket,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  version_->embedded_worker()->BindCacheStorage(std::move(receiver),
                                                bucket.ToBucketLocator());
}

storage::BucketClientInfo ServiceWorkerHost::GetBucketClientInfo() const {
  // TODO(crbug.com/379869738) Remove GetUnsafeValue.
  return storage::BucketClientInfo{worker_process_id().GetUnsafeValue(),
                                   token()};
}

RenderProcessHost* ServiceWorkerHost::GetProcessHost() const {
  return RenderProcessHost::FromID(version_->embedded_worker()->process_id());
}

void ServiceWorkerHost::BindAIManager(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  auto* process = GetProcessHost();
  if (process) {
    GetContentClient()->browser()->BindAIManager(process->GetBrowserContext(),
                                                 this, /*rfh=*/nullptr,
                                                 std::move(receiver));
  }
}

}  // namespace content
