// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_resource_getter_impl.h"

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_message.h"
#include "media/base/android/media_url_interceptor.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/auth.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/http/http_auth.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Returns the cookie manager for the `browser_context` at the client end of the
// mojo pipe. This will be restricted to the origin of `url`, and will apply
// policies from user and ContentBrowserClient to cookie operations.
mojo::PendingRemote<network::mojom::RestrictedCookieManager>
GetRestrictedCookieManagerForContext(
    BrowserContext* browser_context,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    RenderFrameHostImpl* render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  url::Origin request_origin = url::Origin::Create(url);
  StoragePartition* storage_partition =
      browser_context->GetDefaultStoragePartition();

  // `request_origin` cannot be used to create `isolation_info` since it
  // represents the media resource, not the frame origin. Here we use the
  // `top_frame_origin` as the frame origin to ensure the consistency check
  // passes when creating `isolation_info`. This is ok because
  // `isolation_info.frame_origin` is unused in RestrictedCookieManager.
  DCHECK(site_for_cookies.IsNull() ||
         site_for_cookies.IsFirstParty(top_frame_origin.GetURL()));
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, top_frame_origin,
      top_frame_origin, site_for_cookies);

  mojo::PendingRemote<network::mojom::RestrictedCookieManager> pipe;
  static_cast<StoragePartitionImpl*>(storage_partition)
      ->CreateRestrictedCookieManager(
          network::mojom::RestrictedCookieManagerRole::NETWORK, request_origin,
          std::move(isolation_info),
          /* is_service_worker = */ false,
          render_frame_host ? render_frame_host->GetProcess()->GetID() : -1,
          render_frame_host ? render_frame_host->GetRoutingID()
                            : MSG_ROUTING_NONE,
          render_frame_host ? render_frame_host->GetCookieSettingOverrides()
                            : net::CookieSettingOverrides(),
          pipe.InitWithNewPipeAndPassReceiver(),
          render_frame_host ? render_frame_host->CreateCookieAccessObserver()
                            : mojo::NullRemote());
  return pipe;
}

void ReturnResultOnUIThread(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& result) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void ReturnResultOnUIThreadAndClosePipe(
    mojo::Remote<network::mojom::RestrictedCookieManager> pipe,
    base::OnceCallback<void(const std::string&)> callback,
    uint64_t version,
    base::ReadOnlySharedMemoryRegion shared_memory_region,
    const std::string& result) {
  // Clients of GetCookiesString() are free to use |shared_memory_region| and
  // |result| to avoid IPCs when possible. This class has not proven to be a
  // high enough source of IPC traffic to warrant wiring this up. Using them
  // is completely optional so they are simply dropped here.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace

MediaResourceGetterImpl::MediaResourceGetterImpl(
    BrowserContext* browser_context,
    int render_process_id,
    int render_frame_id)
    : browser_context_(browser_context),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}

MediaResourceGetterImpl::~MediaResourceGetterImpl() {}

void MediaResourceGetterImpl::GetAuthCredentials(
    const GURL& url,
    GetAuthCredentialsCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Non-standard URLs, such as data, will not be found in HTTP auth cache
  // anyway, because they have no valid origin, so don't waste the time.
  if (!url.IsStandard()) {
    GetAuthCredentialsCallback(std::move(callback), std::nullopt);
    return;
  }

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id_, render_frame_id_);
  // Can't get a NetworkAnonymizationKey to get credentials if the
  // RenderFrameHost has already been destroyed.
  if (!render_frame_host) {
    GetAuthCredentialsCallback(std::move(callback), std::nullopt);
    return;
  }

  browser_context_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->LookupServerBasicAuthCredentials(
          url, render_frame_host->GetIsolationInfoForSubresources().network_anonymization_key(),
          base::BindOnce(&MediaResourceGetterImpl::GetAuthCredentialsCallback,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaResourceGetterImpl::GetCookies(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    net::StorageAccessApiStatus storage_access_api_status,
    GetCookieCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_,
                                      url::Origin::Create(url))) {
    // Running the callback asynchronously on the caller thread to avoid
    // reentrancy issues.
    ReturnResultOnUIThread(std::move(callback), std::string());
    return;
  }

  mojo::Remote<network::mojom::RestrictedCookieManager> cookie_manager(
      GetRestrictedCookieManagerForContext(
          browser_context_, url, site_for_cookies, top_frame_origin,
          RenderFrameHostImpl::FromID(render_process_id_, render_frame_id_)));
  network::mojom::RestrictedCookieManager* cookie_manager_ptr =
      cookie_manager.get();
  cookie_manager_ptr->GetCookiesString(
      url, site_for_cookies, top_frame_origin, storage_access_api_status,
      /*get_version_shared_memory=*/false, /*is_ad_tagged=*/false,
      /*force_disable_third_party_cookies=*/false,
      base::BindOnce(&ReturnResultOnUIThreadAndClosePipe,
                     std::move(cookie_manager), std::move(callback)));
}

void MediaResourceGetterImpl::GetAuthCredentialsCallback(
    GetAuthCredentialsCB callback,
    const std::optional<net::AuthCredentials>& credentials) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (credentials)
    std::move(callback).Run(credentials->username(), credentials->password());
  else
    std::move(callback).Run(std::u16string(), std::u16string());
}

}  // namespace content
