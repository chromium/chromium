// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/restricted_interest_group_store_impl.h"

#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"

namespace content {

namespace {

constexpr base::TimeDelta kMaxExpiry = base::TimeDelta::FromDays(30);

// Check if `url` can be used as an interest group's ad render URL. Ad URLs can
// be cross origin, unlike other interest group URLs, but are still restricted
// to HTTPS with no embedded credentials.
bool IsUrlAllowedForRenderUrls(const GURL& url) {
  if (url.scheme() != url::kHttpsScheme)
    return false;

  return !url.has_username() && !url.has_password();
}

// Check if `url` can be used with the specified interest group for any of
// script URL, update URL, or realtime data URL. Ad render URLs should be
// checked with IsUrlAllowedForRenderUrls(), which doesn't have the same-origin
// check.
bool IsUrlAllowed(const GURL& url, const blink::mojom::InterestGroup& group) {
  if (url::Origin::Create(url) != group.owner)
    return false;

  // References are allowed in render URLs, since they're loaded in an iframe,
  // but not in other URLs, which are requested directly. References aren't sent
  // in HTTP requests.
  return !url.has_ref() && IsUrlAllowedForRenderUrls(url);
}

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

  if (group->bidding_url && !IsUrlAllowed(*group->bidding_url, *group))
    return;

  if (group->update_url && !IsUrlAllowed(*group->update_url, *group))
    return;

  if (group->trusted_bidding_signals_url) {
    if (!IsUrlAllowed(*group->trusted_bidding_signals_url, *group))
      return;

    // `trusted_bidding_signals_url` must not have a query string, since the
    // query parameter needs to be set as part of running an auction.
    if (group->trusted_bidding_signals_url->has_query())
      return;
  }

  if (group->ads) {
    for (const auto& ad : group->ads.value()) {
      if (!IsUrlAllowedForRenderUrls(ad->render_url))
        return;
    }
  }

  base::Time max_expiry = base::Time::Now() + kMaxExpiry;
  if (group->expiry > max_expiry)
    group->expiry = max_expiry;
  interest_group_manager_.JoinInterestGroup(std::move(group));
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

RestrictedInterestGroupStoreImpl::~RestrictedInterestGroupStoreImpl() = default;

}  // namespace content
