// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/restricted_interest_group_store_impl.h"

#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"

namespace content {

namespace {

constexpr base::TimeDelta kMaxExpiry = base::TimeDelta::FromDays(30);

}  // namespace

RestrictedInterestGroupStoreImpl::RestrictedInterestGroupStoreImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::RestrictedInterestGroupStore> receiver)
    : DocumentServiceBase(render_frame_host, std::move(receiver)),
      interest_group_manager_(*static_cast<StoragePartitionImpl*>(
                                   render_frame_host->GetStoragePartition())
                                   ->GetInterestGroupManager()) {}

// static
void RestrictedInterestGroupStoreImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::RestrictedInterestGroupStore>
        receiver) {
  DCHECK(render_frame_host);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentServiceBase for details.
  new RestrictedInterestGroupStoreImpl(render_frame_host, std::move(receiver));
}

void RestrictedInterestGroupStoreImpl::JoinInterestGroup(
    const blink::InterestGroup& group) {
  // If the interest group API is not allowed for this origin do nothing.
  if (!GetContentClient()->browser()->IsInterestGroupAPIAllowed(
          render_frame_host()->GetBrowserContext(), origin(),
          group.owner.GetURL())) {
    return;
  }

  // Disallow setting interest groups for another origin. Eventually, this will
  // need to perform a fetch to check for cross-origin permissions to add an
  // interest group.
  if (origin() != group.owner)
    return;

  RenderFrameHost* main_frame = render_frame_host()->GetMainFrame();
  GURL main_frame_url = main_frame->GetLastCommittedURL();

  blink::InterestGroup updated_group = group;
  base::Time max_expiry = base::Time::Now() + kMaxExpiry;
  if (updated_group.expiry > max_expiry)
    updated_group.expiry = max_expiry;
  interest_group_manager_.JoinInterestGroup(std::move(updated_group),
                                            main_frame_url);
}

void RestrictedInterestGroupStoreImpl::LeaveInterestGroup(
    const url::Origin& owner,
    const std::string& name) {
  // If the interest group API is not allowed for this origin do nothing.
  if (!GetContentClient()->browser()->IsInterestGroupAPIAllowed(
          render_frame_host()->GetBrowserContext(), origin(), owner.GetURL())) {
    return;
  }

  if (origin().scheme() != url::kHttpsScheme)
    return;

  if (owner != origin())
    return;

  interest_group_manager_.LeaveInterestGroup(owner, name);
}

void RestrictedInterestGroupStoreImpl::UpdateAdInterestGroups() {
  // If the interest group API is not allowed for this origin do nothing.
  if (!GetContentClient()->browser()->IsInterestGroupAPIAllowed(
          render_frame_host()->GetBrowserContext(), origin(),
          origin().GetURL())) {
    return;
  }
  interest_group_manager_.UpdateInterestGroupsOfOwner(origin());
}

RestrictedInterestGroupStoreImpl::~RestrictedInterestGroupStoreImpl() = default;

}  // namespace content
