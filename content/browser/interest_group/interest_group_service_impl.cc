// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_service_impl.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

constexpr base::TimeDelta kMaxExpiry = base::TimeDelta::FromDays(30);

}  // namespace

InterestGroupServiceImpl::InterestGroupServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::RestrictedInterestGroupStore> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)),
      interest_group_manager_(*static_cast<StoragePartitionImpl*>(
                                   render_frame_host->GetStoragePartition())
                                   ->GetInterestGroupStorage()) {}

// static
void InterestGroupServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::RestrictedInterestGroupStore>
        receiver) {
  DCHECK(render_frame_host);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See FrameServiceBase for details.
  new InterestGroupServiceImpl(render_frame_host, std::move(receiver));
}

void InterestGroupServiceImpl::JoinInterestGroup(
    blink::mojom::InterestGroupPtr group) {
  // If the interest group API is not allowed for this origin do nothing.
  if (!GetContentClient()->browser()->IsInterestGroupAPIAllowed(
          render_frame_host()->GetBrowserContext(), origin(),
          group->owner.GetURL())) {
    return;
  }

  // TODO(crbug.com/1200981): Either also check these renderer-side, or report
  // to devtools to get a better error debugging experience.
  if (origin().scheme() != url::kHttpsScheme)
    return;
  if (group->owner != origin())
    return;
  if (group->bidding_url &&
      url::Origin::Create(*group->bidding_url) != origin()) {
    return;
  }
  if (group->update_url &&
      url::Origin::Create(*group->update_url) != origin()) {
    return;
  }
  if (group->trusted_bidding_signals_url &&
      url::Origin::Create(*group->trusted_bidding_signals_url) != origin()) {
    return;
  }
  if (group->ads) {
    for (const auto& ad : group->ads.value()) {
      if (!ad->render_url.SchemeIs(url::kHttpsScheme)) {
        return;
      }
    }
  }
  base::Time max_expiry = base::Time::Now() + kMaxExpiry;
  if (group->expiry > max_expiry)
    group->expiry = max_expiry;
  interest_group_manager_.JoinInterestGroup(std::move(group));
}

void InterestGroupServiceImpl::LeaveInterestGroup(const url::Origin& owner,
                                                  const std::string& name) {
  // If the interest group API is not allowed for this origin do nothing.
  if (!GetContentClient()->browser()->IsInterestGroupAPIAllowed(
          render_frame_host()->GetBrowserContext(), origin(), owner.GetURL())) {
    return;
  }

  if (origin().scheme() != url::kHttpsScheme) {
    return;
  }
  if (owner != origin()) {
    return;
  }
  interest_group_manager_.LeaveInterestGroup(owner, name);
}

InterestGroupServiceImpl::~InterestGroupServiceImpl() = default;

}  // namespace content
