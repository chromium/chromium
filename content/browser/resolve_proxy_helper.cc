// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resolve_proxy_helper.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/network_isolation_key.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

ResolveProxyHelper::ResolveProxyHelper(int render_process_host_id)
    : render_process_host_id_(render_process_host_id) {}

void ResolveProxyHelper::ResolveProxy(const GURL& url,
                                      ResolveProxyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Enqueue the pending request.
  pending_requests_.push_back(PendingRequest(url, std::move(callback)));

  // If nothing is in progress, start.
  if (!receiver_.is_bound()) {
    DCHECK_EQ(1u, pending_requests_.size());
    StartPendingRequest();
  }
}

ResolveProxyHelper::~ResolveProxyHelper() {
  DCHECK(!owned_self_);
  DCHECK(!receiver_.is_bound());
}

void ResolveProxyHelper::StartPendingRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!receiver_.is_bound());
  DCHECK(!pending_requests_.empty());

  // Start the request.
  mojo::PendingRemote<network::mojom::ProxyLookupClient> proxy_lookup_client =
      receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(
      base::BindOnce(&ResolveProxyHelper::OnProxyLookupComplete,
                     base::Unretained(this), net::ERR_ABORTED, base::nullopt));
  owned_self_ = this;
  if (!SendRequestToNetworkService(pending_requests_.front().url,
                                   std::move(proxy_lookup_client))) {
    OnProxyLookupComplete(net::ERR_FAILED, base::nullopt);
  }
}

bool ResolveProxyHelper::SendRequestToNetworkService(
    const GURL& url,
    mojo::PendingRemote<network::mojom::ProxyLookupClient>
        proxy_lookup_client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_host_id_);
  // Fail the request if there's no such RenderProcessHost;
  if (!render_process_host)
    return false;
  // TODO(https://crbug.com/1021661): Pass in a non-empty NetworkIsolationKey.
  render_process_host->GetStoragePartition()
      ->GetNetworkContext()
      ->LookUpProxyForURL(url, net::NetworkIsolationKey::Todo(),
                          std::move(proxy_lookup_client));
  return true;
}

void ResolveProxyHelper::OnProxyLookupComplete(
    int32_t net_error,
    const base::Optional<net::ProxyInfo>& proxy_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!pending_requests_.empty());

  receiver_.reset();

  // Need to keep |this| alive until the end of this method, and then release
  // this reference. StartPendingRequest(), if called, will grab other
  // reference, and a reference may be owned by the IO thread or by other
  // posted tasks, so |this| may or may not be deleted at the end of this
  // method.
  scoped_refptr<ResolveProxyHelper> owned_self = std::move(owned_self_);

  // If all references except |owned_self| have been released, then there's
  // nothing waiting for pending requests to complete. So just exit this method,
  // which will release the last reference, destroying |this|.
  if (HasOneRef())
    return;

  // Clear the current (completed) request.
  PendingRequest completed_req = std::move(pending_requests_.front());
  pending_requests_.pop_front();

  std::move(completed_req.callback)
      .Run(proxy_info ? proxy_info->ToPacString()
                      : base::Optional<std::string>());

  // Start the next request.
  if (!pending_requests_.empty())
    StartPendingRequest();
}

ResolveProxyHelper::PendingRequest::PendingRequest(
    const GURL& url,
    ResolveProxyCallback callback)
    : url(url), callback(std::move(callback)) {}

ResolveProxyHelper::PendingRequest::PendingRequest(
    PendingRequest&& pending_request) noexcept = default;

ResolveProxyHelper::PendingRequest::~PendingRequest() noexcept = default;

ResolveProxyHelper::PendingRequest&
ResolveProxyHelper::PendingRequest::operator=(
    PendingRequest&& pending_request) noexcept = default;

}  // namespace content
