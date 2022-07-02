// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_manager.h"

#include "content/browser/buckets/bucket_manager_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace content {

BucketManager::BucketManager(
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : quota_manager_proxy_(std::move(quota_manager_proxy)) {}

BucketManager::~BucketManager() = default;

void BucketManager::BindReceiverForRenderFrame(
    const GlobalRenderFrameHostId& render_frame_host_id,
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
    mojo::ReportBadMessageCallback bad_message_callback) {
  auto permission_decision = base::BindRepeating(
      [](GlobalRenderFrameHostId id, blink::PermissionType permission_type) {
        auto* rfh = RenderFrameHost::FromID(id);
        if (!rfh)
          return blink::mojom::PermissionStatus::DENIED;
        return rfh->GetBrowserContext()
            ->GetPermissionController()
            ->GetPermissionStatusForCurrentDocument(permission_type, rfh);
      },
      render_frame_host_id);

  RenderFrameHost* rfh = RenderFrameHost::FromID(render_frame_host_id);
  DCHECK(rfh);
  DoBindReceiver(rfh->GetLastCommittedOrigin(), std::move(receiver),
                 permission_decision, std::move(bad_message_callback));
}

void BucketManager::BindReceiverForWorker(
    int render_process_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
    mojo::ReportBadMessageCallback bad_message_callback) {
  auto permission_decision = base::BindRepeating(
      [](int render_process_id, const url::Origin& origin,
         blink::PermissionType permission_type) {
        RenderProcessHost* rph = RenderProcessHost::FromID(render_process_id);
        if (!rph)
          return blink::mojom::PermissionStatus::DENIED;
        return rph->GetBrowserContext()
            ->GetPermissionController()
            ->GetPermissionStatusForWorker(permission_type, rph, origin);
      },
      render_process_id, origin);
  DoBindReceiver(origin, std::move(receiver), permission_decision,
                 std::move(bad_message_callback));
}

void BucketManager::DoBindReceiver(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
    const BucketHost::PermissionDecisionCallback& permission_decision,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = hosts_.find(origin);
  if (it != hosts_.end()) {
    it->second->BindReceiver(std::move(receiver), permission_decision);
    return;
  }

  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    std::move(bad_message_callback)
        .Run("Called Buckets from an insecure context");
    return;
  }

  auto [insert_it, insert_succeeded] = hosts_.insert(
      {origin, std::make_unique<BucketManagerHost>(this, origin)});
  DCHECK(insert_succeeded);
  insert_it->second->BindReceiver(std::move(receiver), permission_decision);
}

void BucketManager::OnHostReceiverDisconnect(BucketManagerHost* host,
                                             base::PassKey<BucketManagerHost>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host != nullptr);
  DCHECK_GT(hosts_.count(host->origin()), 0u);
  DCHECK_EQ(hosts_[host->origin()].get(), host);

  if (host->has_connected_receivers())
    return;

  hosts_.erase(host->origin());
}

}  // namespace content
